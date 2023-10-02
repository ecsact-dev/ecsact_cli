#include "ecsact/cli/report.hh"

static auto _report_handler =
	std::function<void(const ecsact::cli::message_variant_t&)>{};

auto ecsact::cli::report( //
	const message_variant_t& message
) -> void {
	if(_report_handler) {
		_report_handler(message);
	}
}

auto ecsact::cli::set_report_handler( //
	std::function<void(const message_variant_t&)> handler
) -> void {
	_report_handler = handler;
}
