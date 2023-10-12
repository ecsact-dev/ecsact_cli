#pragma once

#include <string>
#include <variant>
#include <vector>
#include <span>
#include <filesystem>
#include <optional>

namespace ecsact {

enum class build_recipe_parse_error {
	bad_file,
	missing_exports,
	expected_map_top_level,
	invalid_source,
	unknown_import_method,
	unknown_export_method,
	conflicting_import_export_method_modules,
};

class build_recipe {
public:
	static auto from_yaml_file( //
		std::filesystem::path p
	) -> std::variant<build_recipe, build_recipe_parse_error>;

	struct source_path {
		std::filesystem::path      path;
		std::optional<std::string> outdir;
	};

	struct source_fetch {
		std::string                url;
		std::optional<std::string> outdir;
	};

	struct source_codegen {
		std::vector<std::string>   plugins;
		std::optional<std::string> outdir;
	};

	using source = std::variant<source_path, source_fetch, source_codegen>;

	build_recipe(build_recipe&&);
	~build_recipe();

	auto exports() const -> std::span<const std::string>;
	auto imports() const -> std::span<const std::string>;
	auto sources() const -> std::span<const source>;
	auto system_libs() const -> std::span<const std::string>;

private:
	std::vector<std::string> _exports;
	std::vector<std::string> _imports;
	std::vector<source>      _sources;
	std::vector<std::string> _system_libs;

	build_recipe();
};
} // namespace ecsact
