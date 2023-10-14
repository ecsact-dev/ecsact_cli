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

// TODO(zaucy): figure out colored output for windows
#ifdef _WIN32
#	define COLOR_RED ""
#	define COLOR_GRN ""
#	define COLOR_YEL ""
#	define COLOR_BLU ""
#	define COLOR_MAG ""
#	define COLOR_CYN ""
#	define COLOR_RESET ""
#else
#	define COLOR_RED "\e[0;31m"
#	define COLOR_GRN "\e[0;32m"
#	define COLOR_YEL "\e[0;33m"
#	define COLOR_BLU "\e[0;34m"
#	define COLOR_MAG "\e[0;35m"
#	define COLOR_CYN "\e[0;36m"
#	define COLOR_RESET "\e[0m"
#endif

namespace {
auto print_text_report(const alert_message& msg) -> void {
	std::cout << std::format( //
		COLOR_RED "ALERT:" COLOR_RESET " {}\n",
		msg.content
	);
}

auto print_text_report(const info_message& msg) -> void {
	std::cout << std::format( //
		COLOR_GRN "INFO:" COLOR_RESET " {}\n",
		msg.content
	);
}

auto print_text_report(const error_message& msg) -> void {
	std::cout << std::format( //
		COLOR_RED "ERROR:" COLOR_RESET " {}\n",
		msg.content
	);
}

auto print_text_report(const ecsact_error_message& msg) -> void {
	std::cerr << std::format( //
		COLOR_RED "ERROR:" COLOR_RESET " {}:{}:{}\n        {}\n",
		msg.ecsact_source_path,
		msg.line,
		msg.character,
		msg.message
	);
}

auto print_text_report(const warning_message& msg) -> void {
	std::cout << std::format( //
		COLOR_YEL "WARNING:" COLOR_RESET " {}\n",
		msg.content
	);
}

auto print_text_report(const success_message& msg) -> void {
	std::cout << std::format( //
		COLOR_GRN "SUCCESS:" COLOR_RESET " {}\n",
		msg.content
	);
}

auto print_text_report(const module_methods_message& msg) -> void {
	std::cout << "MODULE METHODS FOR " << msg.module_name << ":\n";
	for(auto& method : msg.methods) {
		std::cout //
			<< "    " << (method.available ? COLOR_GRN "YES  " : COLOR_RED " NO  ")
			<< COLOR_RESET << method.method_name << "\n";
	}
}

auto print_text_report(const subcommand_start_message& msg) -> void {
	std::cout << std::format( //
		COLOR_BLU "SUBCOMMAND({}) START >>" COLOR_RESET " {} ",
		msg.id,
		msg.executable
	);
	for(auto& arg : msg.arguments) {
		std::cout << arg << " ";
	}
	std::cout << "\n";
}

auto print_text_report(const subcommand_stdout_message& msg) -> void {
	std::cout << std::format( //
		COLOR_BLU "SUBCOMMAND({}) STDOUT:" COLOR_RESET " {}\n",
		msg.id,
		msg.line
	);
}

auto print_text_report(const subcommand_stderr_message& msg) -> void {
	std::cout << std::format( //
		COLOR_RED "SUBCOMMAND({}) STDERR:" COLOR_RESET " {}\n",
		msg.id,
		msg.line
	);
}

auto print_text_report(const subcommand_progress_message& msg) -> void {
	std::cout << std::format( //
		COLOR_BLU "SUBCOMMAND({}) PROG:" COLOR_RESET " {}\n",
		msg.id,
		msg.description
	);
}

auto print_text_report(const subcommand_end_message& msg) -> void {
	std::cout << std::format( //
		COLOR_BLU "SUBCOMMAND({})   END << " COLOR_RESET " exit code {}\n",
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
