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

} // namespace ecsact::cli
