#include "build.hh"

#include <iostream>
#include "docopt.h"
#include "magic_enum.hpp"

#include "./build/build_recipe.hh"

constexpr auto USAGE = R"docopt(Ecsact Build Command

Usage:
	ecsact build (-h | --help)
	ecsact build <files>... --recipe=<name> --output=<path>

Options:
	<files>             Ecsact files used to build Ecsact Runtime
	-r --recipe=<name>  Name or path to recipe
	-o --output=<path>  Runtime output path
)docopt";

auto ecsact::cli::detail::build_command( //
	int   argc,
	char* argv[]
) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});

	auto files = args.at("<files>");
	auto recipe = build_recipe::from_yaml_file(args.at("--recipe").asString());

	if(!recipe) {
		std::cerr << "Recipe Error: " << magic_enum::enum_name(recipe.error()) << "\n";
		return 1;
	}


	return 0;
}
