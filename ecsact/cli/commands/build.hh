
#pragma once

#include <type_traits>

#include "./command.hh"

namespace ecsact::cli::detail {

int build_command(int argc, const char* argv[]);
static_assert(std::is_same_v<command_fn_t, decltype(&build_command)>);

} // namespace ecsact::cli::detail
