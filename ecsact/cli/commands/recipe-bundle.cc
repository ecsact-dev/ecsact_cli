#include "ecsact/cli/commands/recipe-bundle.hh"

#include <memory>
#include <iostream>
#include <format>
#include <filesystem>
#include <variant>
#include <string_view>
#include <format>
#include "docopt.h"
#include "magic_enum.hpp"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/argv0.hh"
#include "ecsact/cli/detail/json_report.hh"
#include "ecsact/cli/detail/text_report.hh"
#include "ecsact/cli/commands/build/build_recipe.hh"

namespace fs = std::filesystem;

using namespace std::string_view_literals;

constexpr auto USAGE = R"docopt(Ecsact Recipe Bundle Command

Usage:
  ecsact recipe-bundle (-h | --help)
  ecsact recipe-bundle <recipe>... --output=<path> [--format=<type>] [--report_filter=<filter>]

Options:
  <recipe>                  Name or path to recipe
  -o --output=<path>        Recipe bundle output path
  -f --format=<type>        The format used to report progress of the build [default: text]
	--report_filter=<filter>  Filtering out report logs [default: none]
)docopt";

// TODO(zaucy): Add this documentation to docopt (msvc regex fails)
// Allowed Formats:
//   none : No output
//   json : Each line of stdout/stderr is a JSON object
//   text : Human readable text format

auto ecsact::cli::detail::recipe_bundle_command( //
	int         argc,
	const char* argv[]
) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto format = args["--format"].asString();
	auto exec_path = canon_argv0(argv[0]);

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

	auto report_filter = args["--report_filter"].asString();
	if(report_filter == "none") {
		set_report_filter(report_filter::none);
	} else if(report_filter == "error_only") {
		set_report_filter(report_filter::error_only);
	} else if(report_filter == "errors_and_warnings") {
		set_report_filter(report_filter::errors_and_warnings);
	} else {
		std::cerr << "Invalid --report_filter value: " << report_filter << "\n";
		std::cout << USAGE;
		return 1;
	}

	auto output_path = fs::path{args.at("--output").asString()};

	auto recipe_composite = std::optional<build_recipe>{};
	auto recipe_paths = args.at("<recipe>").asStringList();
	for(auto recipe_path : args.at("<recipe>").asStringList()) {
		auto recipe_result = build_recipe::from_yaml_file(recipe_path);

		if(std::holds_alternative<build_recipe_parse_error>(recipe_result)) {
			auto recipe_error = std::get<build_recipe_parse_error>(recipe_result);
			ecsact::cli::report_error(
				"Recipe Error {}",
				magic_enum::enum_name(recipe_error)
			);
			return 1;
		}

		auto recipe = std::move(std::get<build_recipe>(recipe_result));
		if(!recipe_composite) {
			recipe_composite.emplace(std::move(recipe));
		} else {
			auto merge_result = build_recipe::merge(*recipe_composite, recipe);
			if(std::holds_alternative<build_recipe_merge_error>(merge_result)) {
				auto merge_error = std::get<build_recipe_merge_error>(merge_result);
				ecsact::cli::report_error(
					"Recipe Merge Error {}",
					magic_enum::enum_name(merge_error)
				);
				return 1;
			}

			recipe_composite.emplace(std::get<build_recipe>(std::move(merge_result)));
		}
	}

	if(!recipe_composite) {
		ecsact::cli::report_error("No recipes");
		return 1;
	}

	auto additional_plugin_dirs = std::vector<fs::path>{};
	for(fs::path recipe_path : recipe_paths) {
		if(recipe_path.has_parent_path()) {
			additional_plugin_dirs.emplace_back(recipe_path.parent_path());
		}
	}

	return 0;
}
