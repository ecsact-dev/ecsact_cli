#pragma once

#include <string>
#include <variant>
#include <vector>
#include <span>
#include <filesystem>
#include <expected>

namespace ecsact {

enum class build_recipe_parse_error {
	bad_file,
	missing_exports,
	expected_map_top_level,
	invalid_source,
};

class build_recipe {
public:
	static auto from_yaml_file( //
		std::filesystem::path p
	) -> std::expected<build_recipe, build_recipe_parse_error>;

	struct source_path {
			std::filesystem::path path;
		};

	struct source_fetch {
		std::string url;
	};

	struct source_codegen {
		std::vector<std::string> plugins;
	};

	using source = std::variant<source_path, source_fetch, source_codegen>;

	build_recipe(build_recipe&&);
	~build_recipe();

	auto exports() const -> std::span<const std::string>;
	auto imports() const -> std::span<const std::string>;
	auto sources() const -> std::span<const source>;

private:
		std::vector<std::string> _exports;
		std::vector<std::string> _imports;
		std::vector<source> _sources;
		
		build_recipe();
};
} // namespace ecsact