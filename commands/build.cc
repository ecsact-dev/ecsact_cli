#include "build.hh"

#include <iostream>
#include <format>
#include <filesystem>
#include <variant>
#include <format>
#include <boost/process.hpp>
#include "docopt.h"
#include "magic_enum.hpp"
#include "ecsact/interpret/eval.hh"
#include "ecsact/runtime/meta.hh"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/json_report.hh"
#include "ecsact/cli/detail/text_report.hh"

#include "commands/build/build_recipe.hh"
#include "commands/build/cc_compiler.hh"

using ecsact::cli::ecsact_error_message;
using ecsact::cli::subcommand_end_message;
using ecsact::cli::subcommand_start_message;

namespace fs = std::filesystem;
namespace bp = boost::process;

constexpr auto USAGE = R"docopt(Ecsact Build Command

Usage:
  ecsact build (-h | --help)
  ecsact build <files>... --recipe=<name> --output=<path> [--format=<type>] [--temp_dir=<path>]

Options:
  <files>             Ecsact files used to build Ecsact Runtime
  -r --recipe=<name>  Name or path to recipe
  -o --output=<path>  Runtime output path
  --temp_dir=<path>   Optional temporary directoy to use instead of generated one
  -f --format=<type>  The format used to report progress of the build [default: text]
                      Allowed Formats:
                        none - No output
                        json - Each line of stdout/stderr is a JSON object
                        text - Human readable text format
)docopt";

auto handle_source( //
	ecsact::build_recipe::source_fetch,
	fs::path work_dir
) -> int {
	ecsact::cli::report_error("Fetching source not yet supported\n");
	return 1;
}

auto handle_source( //
	ecsact::build_recipe::source_codegen,
	fs::path work_dir
) -> int {
	ecsact::cli::report_error("Codegen source not yet supported\n");
	return 1;
}

auto handle_source( //
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

	for(auto lib_dir : options.compiler.std_lib_paths) {
		cl_args.push_back(std::format("/LIBPATH:{}", lib_dir.string()));
	}

	for(auto sys_lib : options.system_libs) {
		cl_args.push_back(std::format("/DEFAULTLIB:{}", sys_lib));
	}

	cl_args.push_back("/DLL");
	cl_args.push_back("/link");
	cl_args.push_back("/MACHINE:x64"); // TODO(zaucy): configure from triple

	cl_args.push_back(std::format("/OUT:{}", options.output_path.string()));

	auto cl_proc = bp::child{
		bp::exe(options.compiler.compiler_path),
		bp::args(cl_args),
	};

	auto compile_subcommand_id = static_cast<ecsact::cli::subcommand_id_t>( //
		cl_proc.id()
	);

	ecsact::cli::report(subcommand_start_message{
		.id = compile_subcommand_id,
		.executable = options.compiler.compiler_path.string(),
		.arguments = cl_args,
	});

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

auto cook_recipe( //
	std::vector<fs::path>       files,
	const ecsact::build_recipe& recipe,
	fs::path                    work_dir,
	fs::path                    output_path
) -> int {
	auto exit_code = int{};

	for(auto& src : recipe.sources()) {
		exit_code =
			std::visit([&](auto& src) { return handle_source(src, work_dir); }, src);

		if(exit_code != 0) {
			break;
		}
	}

	auto compiler = ecsact::detect_cc_compiler();

	if(!compiler) {
		ecsact::cli::report_error(
			"Failed to detect C++ compiler installed on your system\n"
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

	if(is_cl_like(compiler->compiler_type)) {
		auto system_libs = std::vector<std::string>{};
		for(auto sys_lib : recipe.system_libs()) {
			system_libs.push_back(sys_lib);
		}

		cl_compile({
			.work_dir = work_dir,
			.compiler = *compiler,
			.inc_dir = inc_dir,
			.system_libs = system_libs,
			.srcs = source_files,
			.output_path = output_path,
		});
	} else {
		// clang_gcc_compile(*compiler, inc_dir, source_files);
	}

	return exit_code;
}

auto ecsact::cli::detail::build_command( //
	int   argc,
	char* argv[]
) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto format = args["--format"].asString();

	if(format == "text") {
		set_report_handler(text_report{});
	} else if(format == "json") {
		set_report_handler(json_report{});
	} else if(format == "none") {
		set_report_handler({});
	} else {
		std::cerr << "Invalid --format value: " << format << "\n";
		std::cout << USAGE;
		return 1;
	}

	auto files = args.at("<files>").asStringList();
	auto file_paths = std::vector<fs::path>{};
	file_paths.reserve(files.size());

	auto output_path = fs::path{args.at("--output").asString()};

	auto recipe = build_recipe::from_yaml_file(args.at("--recipe").asString());

	auto temp_dir = args["--temp_dir"].isString() //
		? fs::path{args["--temp_dir"].asString()}
		: fs::temp_directory_path();

	// TODO(zaucy): Generate stable directory based on file input
	auto work_dir = temp_dir / "ecsact-build";

	ecsact::cli::report_info(
		"Build Working Directory: {}",
		work_dir.generic_string()
	);

	auto ec = std::error_code{};
	fs::remove_all(work_dir, ec);
	fs::create_directories(work_dir, ec);

	for(auto file : files) {
		if(!fs::exists(file)) {
			ecsact::cli::report_error("Input file {} does not exist\n", file);
			return 1;
		}

		file_paths.emplace_back(file);
	}

	if(std::holds_alternative<build_recipe_parse_error>(recipe)) {
		auto recipe_error = std::get<build_recipe_parse_error>(recipe);
		ecsact::cli::report_error(
			"Recipe Error {}",
			magic_enum::enum_name(recipe_error)
		);
		return 1;
	}

	auto eval_errors = ecsact::eval_files(file_paths);

	if(!eval_errors.empty()) {
		for(auto eval_error : eval_errors) {
			auto err_source_path = file_paths[eval_error.source_index];
			ecsact::cli::report(ecsact_error_message{
				.ecsact_source_path = err_source_path.generic_string(),
				.message = eval_error.error_message,
				.line = eval_error.line,
				.character = eval_error.character,
			});
		}

		return 1;
	}

	auto main_pkg_id = ecsact_meta_main_package();

	if(main_pkg_id == static_cast<ecsact_package_id>(-1)) {
		ecsact::cli::report_error("ecsact build must have main package");
		return 1;
	}

	ecsact::cli::report_info(
		"Main ecsact package: {}",
		ecsact::meta::package_name(main_pkg_id)
	);

	return cook_recipe(
		file_paths,
		std::get<build_recipe>(recipe),
		work_dir,
		output_path
	);
}
