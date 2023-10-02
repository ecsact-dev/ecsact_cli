#include "ecsact/cli/detail/text_report.hh"

#include <iostream>
#include <format>

using ecsact::cli::alert_message;
using ecsact::cli::ecsact_error_message;
using ecsact::cli::error_message;
using ecsact::cli::info_message;
using ecsact::cli::module_methods_message;
using ecsact::cli::subcommand_end_message;
using ecsact::cli::subcommand_progress_message;
using ecsact::cli::subcommand_start_message;
using ecsact::cli::subcommand_stderr_message;
using ecsact::cli::subcommand_stdout_message;
using ecsact::cli::success_message;
using ecsact::cli::warning_message;

namespace {
auto print_text_report(const alert_message& msg) -> void {
	std::cout << std::format( //
		"[ALERT] {}\n",
		msg.content
	);
}

auto print_text_report(const info_message& msg) -> void {
	std::cout << std::format( //
		"[INFO] {}\n",
		msg.content
	);
}

auto print_text_report(const error_message& msg) -> void {
	std::cout << std::format( //
		"[ERROR] {}\n",
		msg.content
	);
}

auto print_text_report(const ecsact_error_message& msg) -> void {
	std::cerr << std::format( //
		"[ERROR] {}:{}:{}\n"
		"        {}\n",
		msg.ecsact_source_path,
		msg.line,
		msg.character,
		msg.message
	);
}

auto print_text_report(const warning_message& msg) -> void {
	std::cout << std::format( //
		"[WARNING] {}\n",
		msg.content
	);
}

auto print_text_report(const success_message& msg) -> void {
	std::cout << std::format( //
		"[SUCCESS] {}\n",
		msg.content
	);
}

auto print_text_report(const module_methods_message& msg) -> void {
	std::cout << "[Module Methods for " << msg.module_name << "]\n";
	for(auto& method : msg.methods) {
		std::cout //
			<< "    " << (method.available ? "YES  " : " NO  ") << method.method_name
			<< "\n";
	}
}

auto print_text_report(const subcommand_start_message& msg) -> void {
	std::cout //
		<< "[SUBCOMMAND START  id=(" << std::to_string(msg.id) << ")] "
		<< msg.executable << " ";
	for(auto& arg : msg.arguments) {
		std::cout << arg << " ";
	}
	std::cout << "\n";
}

auto print_text_report(const subcommand_stdout_message& msg) -> void {
	std::cout << std::format( //
		"[SUBCOMMAND STDOUT id=({})] {}\n",
		msg.id,
		msg.line
	);
}

auto print_text_report(const subcommand_stderr_message& msg) -> void {
	std::cout << std::format( //
		"[SUBCOMMAND STDERR id=({})] {}\n",
		msg.id,
		msg.line
	);
}

auto print_text_report(const subcommand_progress_message& msg) -> void {
	std::cout << std::format( //
		"[SUBCOMMAND PROG   id=({})] {}\n",
		msg.id,
		msg.description
	);
}

auto print_text_report(const subcommand_end_message& msg) -> void {
	std::cout << std::format( //
		"[SUBCOMMAND END    id=({})] exit code {}\n",
		msg.id,
		msg.exit_code
	);
}
} // namespace

auto ecsact::cli::detail::text_report::operator()( //
	const message_variant_t& message
) const -> void {
	std::visit([](const auto& message) { print_text_report(message); }, message);
}
