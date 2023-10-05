#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <optional>

namespace ecsact::cli {

using codegen_output_write_fn_t = void (*)(const char* str, int32_t str_len);

struct codegen_options {
	std::vector<std::filesystem::path> plugin_paths;
	std::filesystem::path              outdir;
	codegen_output_write_fn_t          output_write_fn;
};

auto codegen(codegen_options options) -> int;

auto resolve_plugin_path(
	const std::string&                  plugin_arg,
	const std::filesystem::path&        default_plugins_dir,
	std::vector<std::filesystem::path>& checked_plugin_paths
) -> std::optional<std::filesystem::path>;

auto resolve_plugin_path(
	const std::string&           plugin_arg,
	const std::filesystem::path& default_plugins_dir
) -> std::optional<std::filesystem::path>;

auto current_platform_codegen_plugin_extension() -> std::string;

auto get_default_plugins_dir() -> std::filesystem::path;

} // namespace ecsact::cli
