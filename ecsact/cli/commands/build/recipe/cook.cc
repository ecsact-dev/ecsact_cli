#include "ecsact/cli/commands/build/recipe/cook.hh"

#include <filesystem>
#include <format>
#include <string_view>
#include <string>
#include <fstream>
#include <cstdio>
#include <set>
#include <curl/curl.h>
#undef fopen
#include <boost/url.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include "magic_enum.hpp"
#include "ecsact/cli/detail/proc_exec.hh"
#include "ecsact/cli/commands/build/cc_compiler_config.hh"
#include "ecsact/cli/commands/build/cc_defines_gen.hh"
#include "ecsact/cli/commands/codegen/codegen.hh"
#include "ecsact/cli/commands/codegen/codegen_util.hh"
#include "ecsact/cli/commands/build/get_modules.hh"
#include "ecsact/cli/commands/build/recipe/integrity.hh"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/argv0.hh"
#include "ecsact/cli/detail/download.hh"
#include "ecsact/cli/detail/glob.hh"
#include "ecsact/cli/detail/archive.hh"
#ifndef ECSACT_CLI_USE_SDK_VERSION
#	include "tools/cpp/runfiles/runfiles.h"
#endif

using ecsact::cli::message_variant_t;
using ecsact::cli::report_error;
using ecsact::cli::report_warning;
using ecsact::cli::detail::download_file;
using ecsact::cli::detail::expand_path_globs;
using ecsact::cli::detail::integrity;
using ecsact::cli::detail::path_before_glob;
using ecsact::cli::detail::path_matches_glob;
using ecsact::cli::detail::path_strip_prefix;

namespace fs = std::filesystem;

using namespace std::string_view_literals;
using namespace std::string_literals;

static auto as_vec(auto range) {
	using element_type = std::remove_cvref_t<decltype(range[0])>;
	auto result = std::vector<element_type>{};
	for(auto el : range) {
		result.emplace_back(el);
	}

	return result;
}

static auto fetch_write_file_fn(
	void*  data,
	size_t size,
	size_t nmemb,
	void*  out_file
) -> size_t {
	return fwrite(data, size, nmemb, static_cast<FILE*>(out_file));
}

static auto is_cpp_file(fs::path p) -> bool {
	if(!p.has_extension()) {
		return false;
	}

	constexpr auto valid_extensions = std::array{
		".c",
		".cc",
		".cpp",
		".cxx",
		".c++",
		".C",
		".h",
		".hh",
		".hpp",
		".hxx",
		".ipp",
		".inc",
		".inl",
		".H",
	};

	for(auto extension : valid_extensions) {
		if(p.extension() == extension) {
			return true;
		}
	}

	return false;
}

static auto is_archive(std::string_view pathlike) -> bool {
	constexpr auto valid_suffix = std::array{
		".tar.gz"sv,
		".tgz"sv,
		".zip"sv,
		".tar.xz"sv,
	};

	for(auto suffix : valid_suffix) {
		if(pathlike.ends_with(suffix)) {
			return true;
		}
	}

	return false;
}

static auto write_file(fs::path path, std::span<std::byte> data) -> void {
	auto file = std::ofstream(path, std::ios_base::binary | std::ios_base::trunc);
	assert(file);

	file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

static auto handle_source( //
	fs::path                                base_directory,
	ecsact::build_recipe::source_fetch      src,
	const ecsact::cli::cook_recipe_options& options
) -> int {
	auto outdir = src.outdir //
		? options.work_dir / *src.outdir
		: options.work_dir;
	auto url = boost::url{src.url};
	auto created_out_dirs = std::set<std::string>{};

	auto ensure_dir = [&created_out_dirs](fs::path p) -> int {
		auto dir = p.parent_path();
		if(created_out_dirs.contains(dir.generic_string())) {
			return 0;
		}

		auto ec = std::error_code{};
		fs::create_directories(dir, ec);
		if(ec) {
			ecsact::cli::report_error(
				"failed to create dir {}: {}",
				dir.string(),
				ec.message()
			);
			return 1;
		}

		created_out_dirs.insert(dir.generic_string());
		return 0;
	};

	auto src_integrity = std::optional<integrity>{};
	if(src.integrity) {
		src_integrity = integrity::from_string(*src.integrity);
		if(!*src_integrity) {
			report_error("unknown or invalid integrity string: {}", *src.integrity);
			return 1;
		}
	}

	auto downloaded_data = download_file(src.url);
	if(!downloaded_data) {
		report_error(
			"failed to download {}: {}",
			src.url,
			downloaded_data.error().what()
		);
		return 1;
	}
	auto downloaded_data_integrity = integrity::from_bytes(
		src_integrity ? src_integrity->kind() : integrity::sha256,
		std::span{downloaded_data->data(), downloaded_data->size()}
	);

	if(src_integrity) {
		if(*src_integrity != downloaded_data_integrity) {
			report_warning(
				"{} integrity is {} and was expected to be {}",
				src.url,
				downloaded_data_integrity.to_string(),
				src_integrity->to_string()
			);
			return 1;
		}
	} else {
		report_warning(
			"{} integrity is {}",
			src.url,
			downloaded_data_integrity.to_string()
		);
	}

	if(is_archive(src.url)) {
		ecsact::cli::detail::read_archive(
			*downloaded_data,
			[&](std::string_view path, std::span<std::byte> data) {
				ecsact::cli::report_info(
					"archive path={} data.size={}",
					path,
					data.size()
				);
				if(src.strip_prefix) {
					if(path.starts_with(*src.strip_prefix)) {
						path = path.substr(src.strip_prefix->size());
					}
				}
				if(src.paths) {
					for(auto glob : *src.paths) {
						if(path_matches_glob(path, glob)) {
							auto before_glob = path_before_glob(glob);
							auto rel_outdir = outdir;
							if(auto stripped = path_strip_prefix(path, before_glob)) {
								rel_outdir = outdir / *stripped;
							}
							auto out_file_path = rel_outdir / fs::path{path};
							ensure_dir(out_file_path);
							write_file(out_file_path, data);
						}
					}
				} else {
					auto out_file_path = outdir / fs::path{path};
					ensure_dir(out_file_path);
					write_file(out_file_path, data);
				}
			}
		);
	} else {
		auto path = std::string{url.path().c_str()};
		if(src.strip_prefix) {
			if(path.starts_with(*src.strip_prefix)) {
				path = path.substr(src.strip_prefix->size());
			}
		}
		auto out_file_path = outdir / fs::path{path}.filename();
		ensure_dir(out_file_path);
		write_file(out_file_path, *downloaded_data);
	}

	return 0;
}

static auto handle_source( //
	fs::path                                base_directory,
	ecsact::build_recipe::source_codegen    src,
	const ecsact::cli::cook_recipe_options& options
) -> int {
	auto default_plugins_dir = ecsact::cli::get_default_plugins_dir();
	auto plugin_paths = std::vector<fs::path>{};

	auto out_dir = fs::path(options.work_dir);

	if(src.outdir) {
		auto outdir_path = fs::path(*src.outdir);
		out_dir = options.work_dir / outdir_path;
	}

	for(auto plugin : src.plugins) {
		auto plugin_path = ecsact::cli::resolve_plugin_path({
			.plugin_arg = plugin,
			.default_plugins_dir = default_plugins_dir,
			.additional_plugin_dirs = options.additional_plugin_dirs,
		});

		if(!plugin_path) {
			return 1;
		}

		plugin_paths.push_back(*plugin_path);
	}

	auto exit_code = ecsact::cli::codegen({
		.plugin_paths = plugin_paths,
		.outdir = out_dir,
	});

	return exit_code;
}

static auto handle_source( //
	fs::path                                base_directory,
	ecsact::build_recipe::source_path       src,
	const ecsact::cli::cook_recipe_options& options
) -> int {
	auto outdir = src.outdir //
		? options.work_dir / *src.outdir
		: options.work_dir;

	auto ec = std::error_code{};
	fs::create_directories(outdir, ec);

	auto before_glob = path_before_glob(base_directory / src.path);
	auto paths = expand_path_globs(base_directory / src.path, ec);
	if(ec) {
		ecsact::cli::report_error(
			"Failed to glob {}: {}",
			src.path.generic_string(),
			ec.message()
		);
		return 1;
	}

	for(auto path : paths) {
		if(!fs::exists(base_directory / src.path)) {
			ecsact::cli::report_warning(
				"Current path: {}",
				fs::current_path().generic_string()
			);

			ecsact::cli::report_error(
				"Source file {} does not exist",
				src.path.generic_string()
			);
			return 1;
		}
		auto rel_outdir = outdir;
		if(auto stripped = path_strip_prefix(src.path, before_glob)) {
			rel_outdir = outdir / *stripped;
		}

		fs::create_directories(rel_outdir, ec);
		fs::copy(base_directory / src.path, rel_outdir, ec);

		if(ec) {
			ecsact::cli::report_error(
				"Failed to copy source {} to {}: {}",
				src.path.generic_string(),
				rel_outdir.generic_string(),
				ec.message()
			);
			return 1;
		} else {
			ecsact::cli::report_info(
				"Copied source {} to {}",
				src.path.generic_string(),
				rel_outdir.generic_string()
			);
		}
	}

	return 0;
}

constexpr auto GENERATED_DYLIB_DISCLAIMER = R"(
////////////////////////////////////////////////////////////////////////////////
//                    THIS FILE IS GENERATED - DO NOT EDIT                    //
////////////////////////////////////////////////////////////////////////////////
)";

constexpr auto LOAD_AT_RUNTIME_GUARD = R"(
#ifndef ECSACT_{0}_API_LOAD_AT_RUNTIME
#   error "Expected ECSACT_{0}_API_LOAD_AT_RUNTIME to be set"
#endif // ECSACT_{0}_API_LOAD_AT_RUNTIME

#ifdef ECSACT_{0}_API
#   error "ECSACT_{0}_API may not be set while using generated dylib source"
#endif // ECSACT_{0}_API
)";

static auto generate_dylib_imports( //
	auto&&        imports,
	std::ostream& output
) -> void {
	auto mods = ecsact::cli::detail::get_ecsact_modules(as_vec(imports));
	output << GENERATED_DYLIB_DISCLAIMER;
	output << "#include <string>\n";
	output << "#include \"ecsact/runtime/common.h\"\n";
	for(auto&& [module_name, _] : mods.module_methods) {
		output << std::format("#include \"ecsact/runtime/{}.h\"\n", module_name);
	}

	for(auto&& [module_name, _] : mods.module_methods) {
		output << std::format( //
			LOAD_AT_RUNTIME_GUARD,
			boost::to_upper_copy(module_name)
		);
	}

	output << "\n";

	for(std::string_view imp : imports) {
		output << std::format( //
			"decltype({0})({0}) = nullptr;\n",
			imp
		);
	}

	output << "\n";

	output << //
		"extern \"C\" ECSACT_EXPORT(\"ecsact_dylib_has_fn\") "
		"auto ecsact_dylib_has_fn(const char* fn_name) -> bool {\n";

	for(std::string_view imp : imports) {
		output << std::format(
			"\tif(std::string{{\"{}\"}} == fn_name) return true;\n",
			imp
		);
	}

	output << "\treturn false;\n";
	output << "}\n\n";

	output << //
		"extern \"C\" ECSACT_EXPORT(\"ecsact_dylib_set_fn_addr\") "
		"auto ecsact_dylib_set_fn_addr(const char* fn_name, void(*fn_ptr)()) -> "
		"void {\n";

	for(std::string_view imp : imports) {
		output << std::format(
			"\tif(std::string{{\"{0}\"}} == fn_name) {{ {0} = "
			"reinterpret_cast<decltype({0})>(fn_ptr); return; }}\n",
			imp
		);
	}

	output << "}\n\n";
}

struct compile_options {
	fs::path work_dir;

	ecsact::cli::cc_compiler compiler;
	std::vector<fs::path>    inc_dirs;
	std::vector<std::string> system_libs;
	std::vector<fs::path>    srcs;
	fs::path                 output_path;
	std::vector<std::string> imports;
	std::vector<std::string> exports;
	bool                     debug;
};

auto clang_gcc_compile(compile_options options) -> int {
	const fs::path clang = options.compiler.compiler_path;

	if(clang.empty() || !fs::exists(clang)) {
		ecsact::cli::report_error("Cannot find compiler {}", clang.string());
		return 1;
	}

	auto compile_proc_args = std::vector<std::string>{};

	compile_proc_args.push_back("-c");
	compile_proc_args.push_back("-x");
	compile_proc_args.push_back("c++");

#if !defined(_WIN32)
	compile_proc_args.push_back("-fPIC");
	compile_proc_args.push_back("-fexperimental-library");
#endif
	compile_proc_args.push_back("-std=c++20");
	compile_proc_args.push_back("-stdlib=libc++");

	for(auto inc_dir : options.compiler.std_inc_paths) {
		compile_proc_args.push_back(std::format("-isystem {}", inc_dir.string()));
	}

	for(auto inc_dir : options.inc_dirs) {
		compile_proc_args.push_back("-isystem");
		compile_proc_args.push_back(
			fs::relative(inc_dir, options.work_dir).generic_string()
		);
	}

#ifdef _WIN32
	compile_proc_args.push_back("-D_CRT_SECURE_NO_WARNINGS");
#endif
	compile_proc_args.push_back("-fvisibility=hidden");
	compile_proc_args.push_back("-fvisibility-inlines-hidden");
	compile_proc_args.push_back("-ffunction-sections");
	compile_proc_args.push_back("-fdata-sections");

	auto generated_defines =
		ecsact::cli::cc_defines_gen(options.imports, options.exports);

	if(generated_defines.empty()) {
		ecsact::cli::report_error("no defines (internal error)");
		return 1;
	}

	compile_proc_args.push_back("-DECSACT_BUILD");

	for(auto def : generated_defines) {
		compile_proc_args.push_back(std::format("-D{}", def));
	}

	compile_proc_args.push_back("-O3");

	for(auto src : options.srcs) {
		if(src.extension().string().starts_with(".h")) {
			continue;
		}

		compile_proc_args.push_back(fs::relative(src, options.work_dir).string());
	}

	compile_proc_args.push_back("-static");

	ecsact::cli::report_info("Compiling runtime...");

	auto compile_proc_exit_code = ecsact::cli::detail::spawn_and_report_output(
		clang,
		compile_proc_args,
		options.work_dir
	);

	if(compile_proc_exit_code != 0) {
		ecsact::cli::report_error(
			"Runtime compile failed. Exited with code {}",
			compile_proc_exit_code
		);
		return 1;
	}

	auto link_proc_args = std::vector<std::string>{};

	link_proc_args.push_back("-shared");
#if !defined(_WIN32)
	// link_proc_args.push_back("-Wl,-s");
	link_proc_args.push_back("-Wl,--gc-sections");
	link_proc_args.push_back("-Wl,--exclude-libs,ALL");
#endif

	if(options.compiler.compiler_type == ecsact::cli::cc_compiler_type::clang) {
		link_proc_args.push_back("-fuse-ld=lld");
	}

	link_proc_args.push_back("-o");
	link_proc_args.push_back(
		fs::relative(options.output_path, options.work_dir).string()
	);

	for(auto p : fs::recursive_directory_iterator(options.work_dir)) {
		if(!p.is_regular_file()) {
			continue;
		}

		if(p.path().extension() == ".o") {
			link_proc_args.push_back( //
				fs::relative(p.path(), options.work_dir).string()
			);
		}
	}

	for(auto lib_dir : options.compiler.std_lib_paths) {
		link_proc_args.push_back(std::format("-L{}", lib_dir.string()));
	}

	for(auto sys_lib : options.system_libs) {
		link_proc_args.push_back(std::format("-l{}", sys_lib));
	}

	link_proc_args.push_back("-l:libc++.a");
	link_proc_args.push_back("-l:libc++abi.a");
	link_proc_args.push_back("-rtlib=compiler-rt");
	link_proc_args.push_back("-lpthread");
	link_proc_args.push_back("-ldl");

	ecsact::cli::report_info("Linking runtime...");

	auto link_proc_exit_code = ecsact::cli::detail::spawn_and_report_output(
		clang,
		link_proc_args,
		options.work_dir
	);

	if(link_proc_exit_code != 0) {
		ecsact::cli::report_error(
			"Linking failed. Exited with code {}",
			link_proc_exit_code
		);
		return 1;
	}

	return 0;
}

auto cl_compile(compile_options options) -> int {
	auto abs_from_wd = [&options](fs::path rel_path) {
		assert(!rel_path.empty());
		if(rel_path.is_absolute()) {
			return rel_path;
		}
		return fs::canonical(
			options.work_dir / fs::relative(rel_path, options.work_dir)
		);
	};

	auto cl_args = std::vector<std::string>{};

	cl_args.push_back("/nologo");
	cl_args.push_back("/std:c++20");
	cl_args.push_back("/diagnostics:column");
	cl_args.push_back("/DECSACT_BUILD");

	// TODO(zaucy): Add debug mode
	// if(options.debug) {
	// 	compile_proc_args.push_back("/DEBUG:FULL");
	// 	compile_proc_args.push_back("/MDd");
	// 	compile_proc_args.push_back("/Z7");
	// 	compile_proc_args.push_back("/EHsc");
	// 	compile_proc_args.push_back("/bigobj");
	// }

	// cl_args.push_back("/we4530"); // treat exceptions as errors
	cl_args.push_back("/wd4530"); // ignore use of exceptions warning
	cl_args.push_back("/MD");
	cl_args.push_back("/DNDEBUG");
	cl_args.push_back("/O2");
	cl_args.push_back("/GL");
	cl_args.push_back("/MP");
	cl_args.push_back("/Fo:"); // typos:disable-line
	cl_args.push_back(
		std::format("{}\\", fs::path{options.work_dir}.lexically_normal().string())
	);

	auto generated_defines =
		ecsact::cli::cc_defines_gen(options.imports, options.exports);

	if(generated_defines.empty()) {
		ecsact::cli::report_error("no defines (internal error)");
		return 1;
	}

	for(auto def : generated_defines) {
		cl_args.push_back(std::format("/D{}", def));
	}

	for(auto src : options.srcs) {
		if(src.extension().string().starts_with(".h")) {
			continue;
		}

		cl_args.push_back(abs_from_wd(src).string());
	}

	for(auto inc_dir : options.compiler.std_inc_paths) {
		cl_args.push_back(std::format("/I{}", inc_dir.string()));
	}

	for(auto inc_dir : options.inc_dirs) {
		cl_args.push_back(std::format("/I{}", inc_dir.string()));
	}

	for(auto sys_lib : options.system_libs) {
		cl_args.push_back(std::format("/DEFAULTLIB:{}", sys_lib));
	}

	cl_args.push_back("/link");
	if(options.debug) {
		cl_args.push_back("/DEBUG");
	}
	cl_args.push_back("/DLL");

	for(auto lib_dir : options.compiler.std_lib_paths) {
		cl_args.push_back(std::format("/LIBPATH:{}", lib_dir.string()));
	}

	cl_args.push_back("/MACHINE:x64"); // TODO(zaucy): configure from triple

	cl_args.push_back(std::format("/OUT:{}", options.output_path.string()));

	struct : ecsact::cli::detail::spawn_reporter {
		auto on_std_out(std::string_view line) -> std::optional<message_variant_t> {
			if(line.find(": warning") != std::string::npos) {
				return ecsact::cli::warning_message{
					.content = std::string{line},
				};
			} else if(line.find(": error") != std::string::npos) {
				return ecsact::cli::error_message{
					.content = std::string{line},
				};
			} else if(line.find(": fatal error ") != std::string::npos) {
				return ecsact::cli::error_message{
					.content = std::string{line},
				};
			} else if(line.find(": Command line error ") != std::string::npos) {
				return ecsact::cli::error_message{
					.content = std::string{line},
				};
			}

			return {};
		}

		auto on_std_err(std::string_view line) -> std::optional<message_variant_t> {
			return {};
		}
	} reporter;

	auto compile_exit_code = ecsact::cli::detail::spawn_and_report(
		options.compiler.compiler_path,
		cl_args,
		reporter
	);

	if(compile_exit_code != 0) {
		ecsact::cli::report_error(
			"Failed to compile ecsact runtime. Compiler {} exited with code {}",
			to_string(options.compiler.compiler_type),
			compile_exit_code
		);
		return 1;
	}

	return 0;
}

auto ecsact::cli::cook_recipe( //
	const char*                 argv0,
	const ecsact::build_recipe& recipe,
	cc_compiler                 compiler,
	const cook_recipe_options&  recipe_options
) -> std::optional<std::filesystem::path> {
	auto exit_code = int{};

	for(auto& src : recipe.sources()) {
		exit_code = std::visit(
			[&](auto& src) {
				return handle_source(recipe.base_directory(), src, recipe_options);
			},
			src
		);

		if(exit_code != 0) {
			return {};
		}
	}

	auto output_path = recipe_options.output_path;

	if(output_path.has_extension()) {
		auto has_allowed_output_extension = false;
		for(auto allowed_ext : compiler.allowed_output_extensions) {
			if(allowed_ext == recipe_options.output_path.extension().string()) {
				has_allowed_output_extension = true;
			}
		}

		if(!has_allowed_output_extension) {
			ecsact::cli::report_error(
				"Invalid output extension {}",
				recipe_options.output_path.extension().string()
			);

			return {};
		}

	} else {
		output_path.replace_extension(compiler.preferred_output_extension);
	}

	ecsact::cli::report_info("Compiling {}", output_path.string());

	auto src_dir = recipe_options.work_dir;
	auto inc_dir = recipe_options.work_dir / "include";

	auto inc_dirs = std::vector{inc_dir};

	auto ec = std::error_code{};
	fs::create_directories(inc_dir, ec);
	fs::create_directories(src_dir, ec);

	auto source_files = std::vector<fs::path>{};

	{
		// No need to add to source_files since it will be grabbed in the directory
		// iterator
		auto dylib_src = src_dir / "ecsact-generated-dylib.cc";
		auto dylib_src_stream = std::ofstream{dylib_src};
		generate_dylib_imports(recipe.imports(), dylib_src_stream);
	}

	for(auto entry : fs::recursive_directory_iterator(src_dir)) {
		if(!entry.is_regular_file()) {
			continue;
		}

		if(is_cpp_file(entry)) {
			source_files.emplace_back(entry.path());
		}
	}

	if(source_files.empty()) {
		ecsact::cli::report_error("No source files");
		return {};
	}

#ifndef ECSACT_CLI_USE_SDK_VERSION
	using bazel::tools::cpp::runfiles::Runfiles;
	auto runfiles_error = std::string{};
	auto runfiles = std::unique_ptr<Runfiles>(
		Runfiles::Create(argv0, BAZEL_CURRENT_REPOSITORY, &runfiles_error)
	);
	if(!runfiles) {
		ecsact::cli::report_error("Failed to load runfiles: {}", runfiles_error);
		return {};
	}

	ecsact::cli::report_info("Using ecsact headers from runfiles");

	auto ecsact_runtime_headers_from_runfiles = std::vector<std::string>{
		"ecsact_runtime/ecsact/lib.hh",
		"ecsact_runtime/ecsact/runtime.h",
		"ecsact_runtime/ecsact/runtime/async.h",
		"ecsact_runtime/ecsact/runtime/async.hh",
		"ecsact_runtime/ecsact/runtime/common.h",
		"ecsact_runtime/ecsact/runtime/core.h",
		"ecsact_runtime/ecsact/runtime/core.hh",
		"ecsact_runtime/ecsact/runtime/definitions.h",
		"ecsact_runtime/ecsact/runtime/dylib.h",
		"ecsact_runtime/ecsact/runtime/dynamic.h",
		"ecsact_runtime/ecsact/runtime/meta.h",
		"ecsact_runtime/ecsact/runtime/meta.hh",
		"ecsact_runtime/ecsact/runtime/serialize.h",
		"ecsact_runtime/ecsact/runtime/serialize.hh",
		"ecsact_runtime/ecsact/runtime/static.h",
	};

	for(auto hdr : ecsact_runtime_headers_from_runfiles) {
		auto full_hdr_path = runfiles->Rlocation(hdr);

		if(full_hdr_path.empty()) {
			ecsact::cli::report_error(
				"Cannot find ecsact_runtime header in runfiles: {}",
				hdr
			);
			return {};
		}

		auto rel_hdr_path = hdr.substr("ecsact_runtime/"sv.size());
		fs::create_directories((inc_dir / rel_hdr_path).parent_path(), ec);

		fs::copy_file(full_hdr_path, inc_dir / rel_hdr_path, ec);
		if(ec) {
			ecsact::cli::report_error(
				"Failed to copy ecsact runtime header from runfiles. {} -> {}\n{}",
				full_hdr_path,
				(inc_dir / rel_hdr_path).generic_string(),
				ec.message()
			);
			return {};
		}
	}
#else
	auto exec_path = ecsact::cli::detail::canon_argv0(argv0);
	auto install_prefix = exec_path.parent_path().parent_path();

	inc_dirs.push_back(install_prefix / "include");
#endif

	if(is_cl_like(compiler.compiler_type)) {
		exit_code = cl_compile({
			.work_dir = recipe_options.work_dir,
			.compiler = compiler,
			.inc_dirs = inc_dirs,
			.system_libs = as_vec(recipe.system_libs()),
			.srcs = source_files,
			.output_path = output_path,
			.imports = as_vec(recipe.imports()),
			.exports = as_vec(recipe.exports()),
			.debug = recipe_options.debug,
		});
	} else {
		exit_code = clang_gcc_compile({
			.work_dir = recipe_options.work_dir,
			.compiler = compiler,
			.inc_dirs = inc_dirs,
			.system_libs = as_vec(recipe.system_libs()),
			.srcs = source_files,
			.output_path = output_path,
			.imports = as_vec(recipe.imports()),
			.exports = as_vec(recipe.exports()),
			.debug = recipe_options.debug,
		});
	}

	if(exit_code != 0) {
		return {};
	}

	return output_path;
}
