#include "ecsact/cli/detail/proc_exec.hh"

#include <filesystem>
#include <boost/process.hpp>
#include "ecsact/cli/report.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;

using ecsact::cli::subcommand_end_message;
using ecsact::cli::subcommand_start_message;

auto ecsact::cli::detail::which(std::string_view prog)
	-> std::optional<fs::path> {
	auto result = bp::search_path(prog);

	if(result.empty()) {
		return std::nullopt;
	} else {
		return fs::path{result};
	}
}

auto ecsact::cli::detail::spawn_and_report_output( //
	std::filesystem::path    exe,
	std::vector<std::string> args,
	std::filesystem::path    start_dir
) -> int {
	auto proc_stdout = bp::ipstream{};
	auto proc_stderr = bp::ipstream{};

	auto proc = bp::child{
		bp::exe(fs::absolute(exe).string()),
		bp::start_dir(start_dir.string()),
		bp::args(args),
		bp::std_out > proc_stdout,
		bp::std_err > proc_stderr,
	};

	auto subcommand_id = static_cast<ecsact::cli::subcommand_id_t>( //
		proc.id()
	);

	ecsact::cli::report(subcommand_start_message{
		.id = subcommand_id,
		.executable = exe.string(),
		.arguments = args,
	});

	ecsact::cli::report_stdout(subcommand_id, proc_stdout);
	ecsact::cli::report_stderr(subcommand_id, proc_stderr);

	proc.wait();

	auto proc_exit_code = proc.exit_code();

	ecsact::cli::report(subcommand_end_message{
		.id = subcommand_id,
		.exit_code = proc_exit_code,
	});

	return proc_exit_code;
}

auto ecsact::cli::detail::spawn_get_stdout( //
	std::filesystem::path    exe,
	std::vector<std::string> args,
	fs::path                 start_dir
) -> std::optional<std::string> {
	auto proc_stdout = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(fs::absolute(exe)),
		bp::args(args),
		bp::start_dir(start_dir.string()),
		bp::std_out > proc_stdout,
	};

	auto proc_stdout_string = std::string{};

	while(proc_stdout) {
		auto line = std::string{};
		std::getline(proc_stdout, line);
		proc_stdout_string += line;
	}

	proc.wait();

	if(proc.exit_code() != 0) {
		return std::nullopt;
	}

	return proc_stdout_string;
}
