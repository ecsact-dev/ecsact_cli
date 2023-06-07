#include "build.hh"

constexpr auto USAGE = R"(Ecsact Build Command

Usage:
	ecsact build (-h | --help)
	ecsact build <files>... --output=<path>
)";

constexpr auto OPTIONS = R"(
Options:
	<files>
		Ecsact files used to build Ecsact Runtime
	-o --output=<path>
)";

auto ecsact::cli::detail::build_command( //
	int   argc,
	char* argv[]
) -> int {
	return 0;
}
