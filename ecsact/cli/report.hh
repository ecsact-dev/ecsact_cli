#pragma once

#include "ecsact/cli/report_message.hh"

namespace ecsact::cli {

auto report(const message_variant_t& message) -> void;
auto set_report_handler(std::function<void(const message_variant_t&)>) -> void;

template<typename StringLike>
auto report_error(StringLike message) -> void {
  report(error_message{.content{message}});
}

} // namespace ecsact::cli
