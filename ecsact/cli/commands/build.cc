#include "ecsact/cli/commands/build.hh"

#include <memory>
#include <iostream>
#include <format>
#include <filesystem>
#include <variant>
#include <string_view>
#include <format>
#include "docopt.h"
#include "magic_enum.hpp"
#include "ecsact/interpret/eval.hh"
#include "ecsact/runtime/meta.hh"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/json_report.hh"
#include "ecsact/cli/detail/text_report.hh"
#include "ecsact/cli/commands/build/recipe/taste.hh"
#include "ecsact/cli/commands/build/build_recipe.hh"
#include "ecsact/cli/commands/build/cc_compiler.hh"
#include "ecsact/cli/commands/build/recipe/cook.hh"
#include "ecsact/cli/commands/build/recipe/taste.hh"

using ecsact::cli::ecsact_error_message;
using ecsact::cli::subcommand_end_message;
using ecsact::cli::subcommand_start_message;

namespace fs = std::filesystem;

using namespace std::string_view_literals;

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
		argv[0],
		file_paths,
		std::get<build_recipe>(recipe),
		work_dir,
		output_path
	);
}
