#include "ecsact/cli/commands/build/recipe/cook.hh"

#include <filesystem>
#include <format>
#include <string_view>
#include <boost/process.hpp>
#include "ecsact/cli/commands/build/cc_compiler_config.hh"
#include "ecsact/cli/commands/codegen/codegen.hh"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/argv0.hh"
#ifndef ECSACT_CLI_USE_SDK_VERSION
#	include "tools/cpp/runfiles/runfiles.h"
#endif

namespace fs = std::filesystem;
namespace bp = boost::process;

using namespace std::string_view_literals;

using ecsact::cli::subcommand_end_message;
using ecsact::cli::subcommand_start_message;

static auto handle_source( //
	ecsact::build_recipe::source_fetch,
	fs::path work_dir
) -> int {
	ecsact::cli::report_error("Fetching source not yet supported\n");
	return 1;
}

static auto handle_source( //
	ecsact::build_recipe::source_codegen src,
	fs::path                             work_dir
) -> int {
	auto default_plugins_dir = ecsact::cli::get_default_plugins_dir();
	auto plugin_paths = std::vector<fs::path>{};

	for(auto plugin : src.plugins) {
		auto plugin_path = ecsact::cli::resolve_plugin_path( //
			plugin,
			default_plugins_dir
		);

		if(!plugin_path) {
			return 1;
		}
	}

	auto exit_code = ecsact::cli::codegen({
		.plugin_paths = plugin_paths,
		.outdir = src.outdir ? *src.outdir : "",
	});

	return exit_code;
}

static auto handle_source( //
	ecsact::build_recipe::source_path src,
	fs::path                          work_dir
) -> int {
	auto outdir = src.outdir //
		? work_dir / *src.outdir
		: work_dir;

	auto ec = std::error_code{};
	fs::create_directories(outdir, ec);

	if(!fs::exists(src.path)) {
		ecsact::cli::report_error(
			"Source file {} does not exist",
			src.path.generic_string()
		);
		return 1;
	}

	fs::copy(src.path, outdir, ec);

	if(ec) {
		ecsact::cli::report_error(
			"Failed to copy source {} to {}: {}",
			src.path.generic_string(),
			outdir.generic_string(),
			ec.message()
		);
		return 1;
	}

	return 0;
}

struct compile_options {
	fs::path work_dir;

	ecsact::cli::cc_compiler compiler;
	std::vector<fs::path>    inc_dirs;
	std::vector<std::string> system_libs;
	std::vector<fs::path>    srcs;
	fs::path                 output_path;
};

auto clang_gcc_compile(compile_options options) -> int {
	const fs::path clang = options.compiler.compiler_path;

	if(clang.empty() || !fs::exists(clang)) {
		ecsact::cli::report_error("Cannot find compiler {}", clang.string());
		return 1;
	}

	auto compile_proc_args = std::vector<std::string>{};

	compile_proc_args.push_back("-c");
#if !defined(_WIN32)
	compile_proc_args.push_back("-fPIC");
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

	compile_proc_args.push_back("-isystem"); // TODO(zaucy): why two of these?

#ifdef _WIN32
	compile_proc_args.push_back("-D_CRT_SECURE_NO_WARNINGS");
#endif
	compile_proc_args.push_back("-fvisibility=hidden");
	compile_proc_args.push_back("-fvisibility-inlines-hidden");
	compile_proc_args.push_back("-ffunction-sections");
	compile_proc_args.push_back("-fdata-sections");

	compile_proc_args.push_back("-DECSACT_CORE_API_EXPORT");
	compile_proc_args.push_back("-DECSACT_DYNAMIC_API_EXPORT");
	compile_proc_args.push_back("-DECSACT_STATIC_API_EXPORT");
	compile_proc_args.push_back("-DECSACT_SERIALIZE_API_EXPORT");

	compile_proc_args.push_back("-O3");

	for(auto src : options.srcs) {
		if(src.extension().string().starts_with(".h")) {
			continue;
		}

		compile_proc_args.push_back(fs::relative(src, options.work_dir).string());
	}

	ecsact::cli::report_info("Compiling runtime...");

	auto compile_proc_stdout = bp::ipstream{};
	auto compile_proc_stderr = bp::ipstream{};
	auto compile_proc = bp::child{
		bp::exe(fs::absolute(clang).string()),
		bp::start_dir(options.work_dir.string()),
		bp::args(compile_proc_args),
		bp::std_out > compile_proc_stdout,
		bp::std_err > compile_proc_stderr,
	};

	auto compile_subcommand_id = static_cast<ecsact::cli::subcommand_id_t>( //
		compile_proc.id()
	);

	ecsact::cli::report(subcommand_start_message{
		.id = compile_subcommand_id,
		.executable = clang.string(),
		.arguments = compile_proc_args,
	});

	ecsact::cli::report_stdout(compile_subcommand_id, compile_proc_stdout);
	ecsact::cli::report_stderr(compile_subcommand_id, compile_proc_stderr);

	compile_proc.wait();

	auto compile_proc_exit_code = compile_proc.exit_code();

	ecsact::cli::report(subcommand_end_message{
		.id = compile_subcommand_id,
	});

	if(compile_proc_exit_code != 0) {
		ecsact::cli::report_error(
			"Runtime compoile failed. Exited with code {}",
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

	ecsact::cli::report_info("Linking runtime...");

	auto link_proc_stdout = bp::ipstream{};
	auto link_proc_stderr = bp::ipstream{};
	auto link_proc = bp::child{
		bp::exe(fs::absolute(clang).string()),
		bp::start_dir(options.work_dir.string()),
		bp::args(link_proc_args),
		bp::std_out > link_proc_stdout,
		bp::std_err > link_proc_stderr,
	};

	auto link_subcommand_id = static_cast<ecsact::cli::subcommand_id_t>( //
		link_proc.id()
	);

	ecsact::cli::report(subcommand_start_message{
		.id = link_subcommand_id,
		.executable = clang.string(),
		.arguments = link_proc_args,
	});

	ecsact::cli::report_stdout(link_subcommand_id, link_proc_stdout);
	ecsact::cli::report_stderr(link_subcommand_id, link_proc_stderr);

	link_proc.wait();

	auto link_proc_exit_code = link_proc.exit_code();

	ecsact::cli::report(subcommand_end_message{
		.id = link_subcommand_id,
		.exit_code = link_proc_exit_code,
	});

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

	// TODO(zaucy): Add debug mode
	// if(options.debug) {
	// 	compile_proc_args.push_back("/DEBUG:FULL");
	// 	compile_proc_args.push_back("/MDd");
	// 	compile_proc_args.push_back("/Z7");
	// 	compile_proc_args.push_back("/EHsc");
	// 	compile_proc_args.push_back("/bigobj");
	// }

	cl_args.push_back("/MD");
	cl_args.push_back("/DNDEBUG");
	cl_args.push_back("/O2");
	cl_args.push_back("/GL");
	cl_args.push_back("/MP");

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
	cl_args.push_back("/DLL");

	for(auto lib_dir : options.compiler.std_lib_paths) {
		cl_args.push_back(std::format("/LIBPATH:{}", lib_dir.string()));
	}

	cl_args.push_back("/MACHINE:x64"); // TODO(zaucy): configure from triple

	cl_args.push_back(std::format("/OUT:{}", options.output_path.string()));

	auto cl_proc_stdout = bp::ipstream{};
	auto cl_proc_stderr = bp::ipstream{};
	auto cl_proc = bp::child{
		bp::exe(options.compiler.compiler_path.string()),
		bp::args(cl_args),
		bp::std_out > cl_proc_stdout,
		bp::std_err > cl_proc_stderr,
	};

	auto compile_subcommand_id = static_cast<ecsact::cli::subcommand_id_t>( //
		cl_proc.id()
	);

	ecsact::cli::report(subcommand_start_message{
		.id = compile_subcommand_id,
		.executable = options.compiler.compiler_path.string(),
		.arguments = cl_args,
	});

	ecsact::cli::report_stdout(compile_subcommand_id, cl_proc_stdout);
	ecsact::cli::report_stderr(compile_subcommand_id, cl_proc_stderr);

	cl_proc.wait();

	auto compile_exit_code = cl_proc.exit_code();

	ecsact::cli::report(subcommand_end_message{
		.id = compile_subcommand_id,
		.exit_code = compile_exit_code,
	});

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
	std::vector<fs::path>       files,
	const ecsact::build_recipe& recipe,
	cc_compiler                 compiler,
	fs::path                    work_dir,
	fs::path                    output_path
) -> std::optional<std::filesystem::path> {
	auto exit_code = int{};

	for(auto& src : recipe.sources()) {
		exit_code =
			std::visit([&](auto& src) { return handle_source(src, work_dir); }, src);

		if(exit_code != 0) {
			break;
		}
	}

	if(output_path.has_extension()) {
		auto has_allowed_output_extension = false;
		for(auto allowed_ext : compiler.allowed_output_extensions) {
			if(allowed_ext == output_path.extension().string()) {
				has_allowed_output_extension = true;
			}
		}

		if(!has_allowed_output_extension) {
			ecsact::cli::report_error(
				"Invalid output extension {}",
				output_path.extension().string()
			);

			return {};
		}

	} else {
		output_path.replace_extension(compiler.preferred_output_extension);
	}

	ecsact::cli::report_info("Compiling {}", output_path.string());

	auto src_dir = work_dir / "src";
	auto inc_dir = work_dir / "include";

	auto inc_dirs = std::vector{inc_dir};

	auto ec = std::error_code{};
	fs::create_directories(inc_dir, ec);
	fs::create_directories(src_dir, ec);

	auto source_files = std::vector<fs::path>{};

	for(auto entry : fs::recursive_directory_iterator(src_dir)) {
		if(!entry.is_regular_file()) {
			continue;
		}
		source_files.emplace_back(entry.path());
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

	auto system_libs = std::vector<std::string>{};
	for(auto sys_lib : recipe.system_libs()) {
		system_libs.push_back(sys_lib);
	}

	if(is_cl_like(compiler.compiler_type)) {
		exit_code = cl_compile({
			.work_dir = work_dir,
			.compiler = compiler,
			.inc_dirs = inc_dirs,
			.system_libs = system_libs,
			.srcs = source_files,
			.output_path = output_path,
		});
	} else {
		exit_code = clang_gcc_compile({
			.work_dir = work_dir,
			.compiler = compiler,
			.inc_dirs = inc_dirs,
			.system_libs = system_libs,
			.srcs = source_files,
			.output_path = output_path,
		});
	}

	if(exit_code != 0) {
		return {};
	}

	return output_path;
}
