#pragma once

#include <format>
#include "ecsact/cli/report_message.hh"

namespace ecsact::cli {

auto report(const message_variant_t& message) -> void;
auto set_report_handler(std::function<void(const message_variant_t&)>) -> void;

template<typename... Args>
auto report_error(std::format_string<Args...> fmt, Args&&... args) -> void {
	report(error_message{
		.content = std::format<Args...>(fmt, std::forward<Args>(args)...),
	});
}

template<typename... Args>
auto report_info(std::format_string<Args...> fmt, Args&&... args) -> void {
	report(info_message{
		.content = std::format<Args...>(fmt, std::forward<Args>(args)...),
	});
}

template<typename... Args>
auto report_warning(std::format_string<Args...> fmt, Args&&... args) -> void {
	report(warning_message{
		.content = std::format<Args...>(fmt, std::forward<Args>(args)...),
	});
}

template<typename... Args>
auto report_success(std::format_string<Args...> fmt, Args&&... args) -> void {
	report(success_message{
		.content = std::format<Args...>(fmt, std::forward<Args>(args)...),
	});
}

} // namespace ecsact::cli
