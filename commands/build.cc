#include "build.hh"

#include <iostream>
#include <format>
#include <filesystem>
#include <variant>
#include <format>
#include "docopt.h"
#include "magic_enum.hpp"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/json_report.hh"
#include "ecsact/cli/detail/text_report.hh"

#include "commands/build/build_recipe.hh"
#include "commands/build/cc_compiler.hh"

namespace fs = std::filesystem;

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

auto cook_recipe( //
	std::vector<fs::path>       files,
	const ecsact::build_recipe& recipe,
	fs::path                    work_dir
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

	auto recipe = build_recipe::from_yaml_file(args.at("--recipe").asString());

	auto temp_dir = args["temp_dir"].isString() //
		? fs::path{args["temp_dir"].asString()}
		: fs::temp_directory_path();

	// TODO(zaucy): Generate stable directory based on file input
	auto work_dir = temp_dir / "ecsact-build";

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

	return cook_recipe(file_paths, std::get<build_recipe>(recipe), work_dir);
}
