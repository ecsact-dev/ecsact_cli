#pragma once

#include "ecsact/cli/report_message.hh"

namespace ecsact::cli::detail {
struct json_report {
	auto operator()(const message_variant_t&) const -> void;
};
} // namespace ecsact::cli::detail
