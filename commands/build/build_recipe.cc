#include "build_recipe.hh"

#include <fstream>
#include <cassert>
#include "yaml-cpp/yaml.h"

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

static auto parse_sources( //
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

			if((codegen && fetch) || (!codegen || !fetch)) {
				return ecsact::build_recipe_parse_error::invalid_source;
			}

			if(codegen) {
				result.emplace_back(source_codegen{
					.plugins = codegen.as<std::vector<std::string>>(),
				});
			} else if(fetch) {
				result.emplace_back(source_fetch{
					.url = fetch.as<std::string>(),
				});
			}
		} else if(src.IsScalar()) {
			auto path = src.as<std::string>();
			result.emplace_back(source_path{path});
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

		auto sources = parse_sources(doc["sources"]);
		if(auto err = get_if_error(sources)) {
			return *err;
		}

		auto recipe = build_recipe{};
		recipe._exports = get_value(exports);
		recipe._imports = get_value(imports);
		recipe._sources = get_value(sources);

		return recipe;
	} catch(const YAML::BadFile&) {
		return build_recipe_parse_error::bad_file;
	}
}
