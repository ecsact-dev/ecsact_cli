#pragma once

#include "ecsact/cli/commands/build/build_recipe.hh"

namespace ecsact {

class build_recipe_bundle {
public:
	static auto create(const build_recipe& recipe) -> build_recipe_bundle;
};

} // namespace ecsact
