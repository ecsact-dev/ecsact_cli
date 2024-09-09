#include "ecsact/cli/commands/recipe-bundle.hh"

#include <memory>
#include <iostream>
#include <format>
#include <filesystem>
#include <variant>
#include <string_view>
#include <format>
#include <fstream>
#include "codegen/codegen_util.hh"
#include "docopt.h"
#include "magic_enum.hpp"
#include "ecsact/cli/report.hh"
#include "ecsact/cli/detail/argv0.hh"
#include "ecsact/cli/detail/json_report.hh"
#include "ecsact/cli/detail/text_report.hh"
#include "ecsact/cli/commands/common.hh"
#include "ecsact/cli/commands/build/build_recipe.hh"
#include "ecsact/cli/commands/recipe-bundle/build_recipe_bundle.hh"
#include "ecsact/codegen/plugin_validate.hh"

namespace fs = std::filesystem;

using namespace std::string_view_literals;
using namespace std::string_literals;

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

static auto write_file( //
	fs::path                   path,
	std::span<const std::byte> bytes
) -> bool {
	auto file = std::ofstream{path, std::ios_base::binary};
	file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	return true;
}

auto ecsact::cli::detail::recipe_bundle_command( //
	int         argc,
	const char* argv[]
) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	if(auto exit_code = process_common_args(args); exit_code != 0) {
		return exit_code;
	}

	auto exec_path = canon_argv0(argv[0]);
	auto output_path = fs::path{args.at("--output").asString()};

	if(output_path.has_extension()) {
		if(output_path.extension() != ".ecsact-recipe-bundle") {
			ecsact::cli::report_error(
				"Output path extension {} is not allowed. Did you mean '--output={}'?",
				output_path.extension().generic_string(),
				fs::path{output_path}
					.replace_extension("")
					.replace_extension("")
					.replace_extension("ecsact-recipe-bundle")
					.filename()
					.generic_string()
			);
			return 1;
		}
	} else {
		output_path.replace_extension("ecsact-recipe-bundle");
	}

	auto recipe_composite = std::optional<build_recipe>{};
	auto recipe_paths = args.at("<recipe>").asStringList();

	auto additional_plugin_dirs = std::vector<fs::path>{};
	for(fs::path recipe_path : recipe_paths) {
		if(recipe_path.has_parent_path()) {
			additional_plugin_dirs.emplace_back(recipe_path.parent_path());
		}
	}

	for(auto recipe_path : recipe_paths) {
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

		for(auto& source : recipe.sources()) {
			auto result = std::get_if<build_recipe::source_codegen>(&source);
			if(result) {
				if(result->plugins.empty()) {
					ecsact::cli::report_error(
						"Recipe source in recipe bundle has no plugins {}",
						recipe_path
					);
					return 1;
				}

				for(auto plugin : result->plugins) {
					auto checked_plugin_paths = std::vector<fs::path>{};
					auto plugin_path = resolve_plugin_path(
						{.plugin_arg = plugin,
						 .default_plugins_dir = get_default_plugins_dir(),
						 .additional_plugin_dirs = additional_plugin_dirs},
						checked_plugin_paths
					);
					if(plugin_path) {
						auto validate_result =
							ecsact::codegen::plugin_validate(*plugin_path);

						if(!validate_result.ok()) {
							auto err_msg = "Plugin validation failed for '" + plugin + "'\n";
							for(auto err : validate_result.errors) {
								err_msg += " - "s + to_string(err) + "\n";
							}
							ecsact::cli::report_error("{}", err_msg);
							return 1;
						}
					} else {
						auto err_msg = "Unable to find codegen plugin '" + plugin +
							"'. Paths checked:\n";
						for(auto& checked_path : checked_plugin_paths) {
							err_msg += " - " + fs::relative(checked_path).string() + "\n";
						}
						ecsact::cli::report_error("{}", err_msg);
					}
				}
			}

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

				recipe_composite.emplace(std::get<build_recipe>(std::move(merge_result))
				);
			}
		}

		if(!recipe_composite) {
			ecsact::cli::report_error("No recipes");
			return 1;
		}

		auto bundle = ecsact::build_recipe_bundle::create(*recipe_composite);

		if(!bundle) {
			ecsact::cli::report_error(
				"Failed to create recipe bundle {}",
				bundle.error().what()
			);
			return 1;
		}

		if(!write_file(output_path, bundle->bytes())) {
			return 1;
		}

		ecsact::cli::report_info(
			"Created bundle {}",
			fs::absolute(output_path).string()
		);

		return 0;
	}
}
