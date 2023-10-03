#pragma once

#include <optional>
#include <filesystem>
#include <string>
#include <cstdint>
#include <format>
#include "magic_enum.hpp"

namespace ecsact {
// clang-format off
enum class cc_compiler_type : std::uint32_t {
  unknown =         0b0000'0000'0000'0000,

	////////////////////
	// Common flags

  gcc_clang_like =  0b0000'0000'0000'0001, /// Has clang/gcc compiler flags
  cl_like =         0b0000'0000'0000'0010, /// Has msvc cl compiler flags

	////////////////////////
	// Specific compilers

  msvc_cl =         0b0000'0001'0000'0010, /// Microsoft cl.exe compiler
  clang_cl =        0b0000'0010'0000'0010, /// Clangs msvc compatiblity compiler
  clang =           0b0000'0011'0000'0001, /// Clang compiler
  gcc =             0b0000'0101'0000'0001, /// GNU compiler
	emcc =            0b0000'0111'0000'0001, /// Emscripten compiler
};

// clang-format on

auto to_string(cc_compiler_type type) -> std::string_view;

constexpr auto is_cl_like(cc_compiler_type type) -> bool {
	return static_cast<std::uint32_t>(type) &
		static_cast<std::uint32_t>(cc_compiler_type::cl_like);
}

static_assert(is_cl_like(cc_compiler_type::msvc_cl));
static_assert(is_cl_like(cc_compiler_type::clang_cl));
static_assert(!is_cl_like(cc_compiler_type::clang));
static_assert(!is_cl_like(cc_compiler_type::gcc));
static_assert(!is_cl_like(cc_compiler_type::emcc));

constexpr auto is_gcc_clang_like(cc_compiler_type type) -> bool {
	return static_cast<std::uint32_t>(type) &
		static_cast<std::uint32_t>(cc_compiler_type::gcc_clang_like);
}

static_assert(is_gcc_clang_like(cc_compiler_type::clang));
static_assert(is_gcc_clang_like(cc_compiler_type::gcc));
static_assert(is_gcc_clang_like(cc_compiler_type::emcc));
static_assert(!is_gcc_clang_like(cc_compiler_type::msvc_cl));
static_assert(!is_gcc_clang_like(cc_compiler_type::clang_cl));

struct cc_compiler {
	cc_compiler_type      compiler_type;
	std::filesystem::path compiler_path;
	std::string           compiler_version;
	std::filesystem::path install_path;

	std::vector<std::filesystem::path> std_inc_paths;
	std::vector<std::filesystem::path> std_lib_paths;

	std::string              preferred_output_extension;
	std::vector<std::string> allowed_output_extensions;
};

auto detect_cc_compiler( //
	std::filesystem::path work_dir
) -> std::optional<cc_compiler>;
} // namespace ecsact
