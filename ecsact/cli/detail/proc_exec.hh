#pragma once

#include <filesystem>
#include <vector>
#include <string_view>
#include <string>
#include <optional>

namespace ecsact::cli::detail {

/**
 * Lookup program on PATH
 */
auto which( //
	std::string_view prog
) -> std::optional<std::filesystem::path>;

/**
 * Spawn a process and report the stdout/stderr
 * @returns exit code
 */
auto spawn_and_report_output( //
	std::filesystem::path    exe,
	std::vector<std::string> args,
	std::filesystem::path    start_dir = std::filesystem::current_path()
) -> int;

/**
 * Spawn a process and return the stdout
 * @returns `std::nullopt` if spawn failed
 */
auto spawn_get_stdout( //
	std::filesystem::path    exe,
	std::vector<std::string> args,
	std::filesystem::path    start_dir = std::filesystem::current_path()
) -> std::optional<std::string>;
} // namespace ecsact::cli::detail
