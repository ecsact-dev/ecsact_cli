#pragma once

#include <filesystem>
#include <optional>
#include "ecsact/cli/commands/build/build_recipe.hh"
#include "ecsact/cli/commands/build/cc_compiler_config.hh"

namespace ecsact::cli {

/**
 * @returns the cooked runtime path
 */
auto cook_recipe(
	const char*                        argv0,
	std::vector<std::filesystem::path> files,
	const ecsact::build_recipe&        recipe,
	cc_compiler                        compiler,
	std::filesystem::path              work_dir,
	std::filesystem::path              output_path
) -> std::optional<std::filesystem::path>;

} // namespace ecsact::cli
