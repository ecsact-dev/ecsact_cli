#include "build.hh"

#include <iostream>
#include <format>
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


static auto start_build_recipe(
	const ecsact::build_recipe& recipe
) -> int {
	return 0;
}

auto ecsact::cli::detail::build_command( //
	int   argc,
	char* argv[]
) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});

	auto files = args.at("<files>");
	auto recipe = build_recipe::from_yaml_file(args.at("--recipe").asString());

	if(std::holds_alternative<build_recipe_parse_error>(recipe)) {
		auto recipe_error = std::get<build_recipe_parse_error>(recipe);
		std::cerr << std::format(
			"Recipe Error: {}\n",
			magic_enum::enum_name(recipe_error)
		);
		return 1;
	}

	return start_build_recipe(std::get<build_recipe>(recipe));
}
