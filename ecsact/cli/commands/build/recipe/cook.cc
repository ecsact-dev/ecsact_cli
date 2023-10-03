#include "ecsact/cli/commands/build/recipe/cook.hh"

#include <filesystem>
#include <format>
#include <string_view>
#include <boost/process.hpp>
#include "ecsact/cli/commands/build/cc_compiler.hh"
#include "ecsact/cli/report.hh"
#ifndef ECSACT_CLI_USE_SDK_VERSION
#	include "tools/cpp/runfiles/runfiles.h"
#endif

namespace fs = std::filesystem;
namespace bp = boost::process;

using namespace std::string_view_literals;

using ecsact::cli::subcommand_start_message;
using ecsact::cli::subcommand_end_message;

static auto handle_source( //
	ecsact::build_recipe::source_fetch,
	fs::path work_dir
) -> int {
	ecsact::cli::report_error("Fetching source not yet supported\n");
	return 1;
}

static auto handle_source( //
	ecsact::build_recipe::source_codegen,
	fs::path work_dir
) -> int {
	ecsact::cli::report_error("Codegen source not yet supported\n");
	return 1;
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

	ecsact::cc_compiler      compiler;
	fs::path                 inc_dir;
	std::vector<std::string> system_libs;
	std::vector<fs::path>    srcs;
	fs::path                 output_path;
};

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

	cl_args.push_back(std::format("/I{}", options.inc_dir.string()));

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
		bp::exe(options.compiler.compiler_path),
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
			options.compiler.compiler_type,
			compile_exit_code
		);
		return 1;
	}

	return 0;
}

auto ecsact::cli::cook_recipe( //
	[[maybe_unused]] const char* argv0,
	std::vector<fs::path>        files,
	const ecsact::build_recipe&  recipe,
	fs::path                     work_dir,
	fs::path                     output_path
) -> int {
	auto exit_code = int{};

	for(auto& src : recipe.sources()) {
		exit_code =
			std::visit([&](auto& src) { return handle_source(src, work_dir); }, src);

		if(exit_code != 0) {
			break;
		}
	}

	auto compiler = ecsact::detect_cc_compiler(work_dir);

	if(!compiler) {
		ecsact::cli::report_error(
			"Failed to detect C++ compiler installed on your system"
		);
		return 1;
	}

	ecsact::cli::report_info(
		"Using compiler: {} ({})",
		compiler->compiler_type,
		compiler->compiler_version
	);

	auto src_dir = work_dir / "src";
	auto inc_dir = work_dir / "include";

	auto ec = std::error_code{};
	fs::create_directories(inc_dir, ec);

	auto source_files = std::vector<fs::path>{};

	for(auto entry : fs::recursive_directory_iterator(src_dir)) {
		if(!entry.is_regular_file()) {
			continue;
		}
		source_files.emplace_back(entry.path());
	}

	if(source_files.empty()) {
		ecsact::cli::report_error("No source files");
		return 1;
	}

#ifndef ECSACT_CLI_USE_SDK_VERSION
	using bazel::tools::cpp::runfiles::Runfiles;
	auto runfiles_error = std::string{};
	auto runfiles = std::unique_ptr<Runfiles>(
		Runfiles::Create(argv0, BAZEL_CURRENT_REPOSITORY, &runfiles_error)
	);
	if(!runfiles) {
		ecsact::cli::report_error("Failed to load runfiles: {}", runfiles_error);
		return 1;
	}

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
			return 1;
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
			return 1;
		}
	}
#endif

	if(is_cl_like(compiler->compiler_type)) {
		auto system_libs = std::vector<std::string>{};
		for(auto sys_lib : recipe.system_libs()) {
			system_libs.push_back(sys_lib);
		}

		exit_code = cl_compile({
			.work_dir = work_dir,
			.compiler = *compiler,
			.inc_dir = inc_dir,
			.system_libs = system_libs,
			.srcs = source_files,
			.output_path = output_path,
		});
	} else {
		// TODO(zaucy): gcc/clang like compile
		// clang_gcc_compile(*compiler, inc_dir, source_files);
	}

	if(exit_code != 0) {
		return exit_code;
	}

	return exit_code;
}
