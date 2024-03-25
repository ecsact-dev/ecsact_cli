#include "ecsact/cli/commands/build/build_recipe.hh"

#include <fstream>
#include <filesystem>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <ranges>
#include "yaml-cpp/yaml.h"
#include "ecsact/cli/commands/build/get_modules.hh"

namespace fs = std::filesystem;

ecsact::build_recipe::build_recipe() = default;
ecsact::build_recipe::build_recipe(build_recipe&&) = default;
ecsact::build_recipe::~build_recipe() = default;

auto ecsact::build_recipe::exports() const -> std::span<const std::string> {
	return _exports;
}

auto ecsact::build_recipe::imports() const -> std::span<const std::string> {
	return _imports;
}

auto ecsact::build_recipe::sources() const -> std::span<const source> {
	return _sources;
}

auto ecsact::build_recipe::system_libs() const -> std::span<const std::string> {
	return _system_libs;
}

static auto get_if_error( //
	const auto& result
) -> std::optional<ecsact::build_recipe_parse_error> {
	if(auto err = std::get_if<ecsact::build_recipe_parse_error>(&result)) {
		return *err;
	} else {
		return {};
	}
}

static auto get_value(auto& result) {
	auto value = std::get_if<0>(&result);
	// If you're using get_value you should have already did get_if_error
	assert(value != nullptr);
	return *value;
}

static auto parse_exports( //
	YAML::Node exports
) -> std::variant<std::vector<std::string>, ecsact::build_recipe_parse_error> {
	if(!exports.IsSequence()) {
		return ecsact::build_recipe_parse_error::missing_exports;
	}

	return exports.as<std::vector<std::string>>();
}

static auto parse_imports( //
	YAML::Node imports
) -> std::variant<std::vector<std::string>, ecsact::build_recipe_parse_error> {
	if(!imports) {
		return {};
	}

	return imports.as<std::vector<std::string>>();
}

static auto parse_system_libs( //
	YAML::Node system_libs
) -> std::variant<std::vector<std::string>, ecsact::build_recipe_parse_error> {
	if(!system_libs) {
		return {};
	}

	return system_libs.as<std::vector<std::string>>();
}

static auto parse_sources( //
	fs::path   recipe_path,
	YAML::Node sources
)
	-> std::variant<
		std::vector<ecsact::build_recipe::source>,
		ecsact::build_recipe_parse_error> {
	using source = ecsact::build_recipe::source;
	using source_fetch = ecsact::build_recipe::source_fetch;
	using source_codegen = ecsact::build_recipe::source_codegen;
	using source_path = ecsact::build_recipe::source_path;

	auto result = std::vector<ecsact::build_recipe::source>{};

	for(auto src : sources) {
		if(src.IsMap()) {
			auto codegen = src["codegen"];
			auto fetch = src["fetch"];
			auto path = src["path"];

			if((codegen && fetch) || (codegen && path) || (fetch && path) ||
				 (!codegen && !fetch && !path)) {
				return ecsact::build_recipe_parse_error::invalid_source;
			}

			if(codegen) {
				auto entry = source_codegen{};
				if(src["outdir"]) {
					entry.outdir = src["outdir"].as<std::string>();
				}
				if(codegen.IsSequence()) {
					entry.plugins = codegen.as<std::vector<std::string>>();
				} else {
					entry.plugins.push_back(codegen.as<std::string>());
				}
				result.emplace_back(entry);
			} else if(fetch) {
				auto outdir = std::optional<std::string>{};
				if(src["outdir"]) {
					outdir = src["outdir"].as<std::string>();
				}
				result.emplace_back(source_fetch{
					.url = fetch.as<std::string>(),
   					.outdir = outdir,
				});
			} else if(path) {
				auto src_path = fs::path{path.as<std::string>()};
				auto outdir = std::optional<std::string>{};
				auto relative_to_cwd = src["relative_to_cwd"] //
					? src["relative_to_cwd"].as<bool>()
					: false;
				if(src["outdir"]) {
					outdir = src["outdir"].as<std::string>();
				}

				if(!relative_to_cwd && !src_path.is_absolute()) {
					src_path = recipe_path.parent_path() / src_path;
				}

				result.emplace_back(source_path{
					.path = src_path,
					.outdir = outdir,
				});
			}
		} else if(src.IsScalar()) {
			auto path = fs::path{src.as<std::string>()};
			if(!path.is_absolute()) {
				path = recipe_path.parent_path() / path;
			}
			result.emplace_back(source_path{
				.path = path,
				.outdir = "src",
			});
		}
	}

	return result;
}

auto ecsact::build_recipe::from_yaml_file( //
	std::filesystem::path p
) -> std::variant<build_recipe, build_recipe_parse_error> {
	auto file = std::ifstream{p};
	try {
		auto doc = YAML::LoadFile(p.string());

		if(!doc.IsMap()) {
			return build_recipe_parse_error::expected_map_top_level;
		}

		auto exports = parse_exports(doc["exports"]);
		if(auto err = get_if_error(exports)) {
			return *err;
		}

		auto imports = parse_imports(doc["imports"]);
		if(auto err = get_if_error(imports)) {
			return *err;
		}

		auto sources = parse_sources(p, doc["sources"]);
		if(auto err = get_if_error(sources)) {
			return *err;
		}

		auto system_libs = parse_system_libs(doc["system_libs"]);
		if(auto err = get_if_error(system_libs)) {
			return *err;
		}

		auto recipe = build_recipe{};
		recipe._exports = get_value(exports);
		recipe._imports = get_value(imports);
		recipe._sources = get_value(sources);
		recipe._system_libs = get_value(system_libs);

		if(recipe._exports.empty()) {
			return build_recipe_parse_error::missing_exports;
		}

		auto import_modules =
			ecsact::cli::detail::get_ecsact_modules(recipe._imports);
		auto export_modules =
			ecsact::cli::detail::get_ecsact_modules(recipe._exports);

		if(!import_modules.unknown_module_methods.empty()) {
			return build_recipe_parse_error::unknown_import_method;
		}

		if(!export_modules.unknown_module_methods.empty()) {
			return build_recipe_parse_error::unknown_export_method;
		}

		for(auto&& [imp_mod, _] : import_modules.module_methods) {
			for(auto&& [exp_mod, _] : export_modules.module_methods) {
				if(imp_mod == exp_mod) {
					return build_recipe_parse_error::
						conflicting_import_export_method_modules;
				}
			}
		}

		return recipe;
	} catch(const YAML::BadFile&) {
		return build_recipe_parse_error::bad_file;
	}
}
