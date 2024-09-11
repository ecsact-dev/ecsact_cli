#include "gtest/gtest.h"
#include <iostream>
#include <ranges>
#include <cassert>
#include "ecsact/cli/commands/build/build_recipe.hh"

constexpr auto RECIPE_A = R"yaml(
name: A
sources:
- x/y/z.cc

imports: []
exports: [ecsact_create_registry]
)yaml";

constexpr auto RECIPE_B = R"yaml(
name: B
sources:
- bbb/b.cc

imports: []
exports: [ecsact_destroy_registry]
)yaml";

constexpr auto RECIPE_C = R"yaml(
name: C
sources:
- x/y/v/d.cc

imports: []
exports: [ecsact_clear_registry]
)yaml";

auto contains_source_path(auto&& r, auto p) -> bool {
	using std::ranges::find_if;

	auto itr = find_if(r, [&](auto&& src) -> bool {
		auto src_path = std::get_if<ecsact::build_recipe::source_path>(&src);
		if(!src_path) {
			return false;
		}

		return src_path->path.generic_string() == p;
	});

	return itr != r.end();
}

auto sources_path_str(auto&& r) -> std::string {
	auto str = std::string{};

	for(auto&& src : r) {
		auto src_path = std::get_if<ecsact::build_recipe::source_path>(&src);
		if(!src_path) {
			continue;
		}
		str += "  " + src_path->path.generic_string() + "\n";
	}

	return str;
}

TEST(RecipeMerge, Correct) {
	using std::ranges::find;
	using std::ranges::find_if;

	auto a = std::get<ecsact::build_recipe>(
		ecsact::build_recipe::from_yaml_string(RECIPE_A, "z/a.yml")
	);
	auto b = std::get<ecsact::build_recipe>(
		ecsact::build_recipe::from_yaml_string(RECIPE_B, "b.yml")
	);
	auto c = std::get<ecsact::build_recipe>(
		ecsact::build_recipe::from_yaml_string(RECIPE_C, "z/b/q/c.yml")
	);

	auto ab_m = std::get<ecsact::build_recipe>(ecsact::build_recipe::merge(a, b));

	EXPECT_EQ(ab_m.base_directory(), a.base_directory());

	EXPECT_TRUE(contains_source_path(ab_m.sources(), "z/x/y/z.cc"))
		<< "Found:\n"
		<< sources_path_str(ab_m.sources());

	EXPECT_TRUE(contains_source_path(ab_m.sources(), "../bbb/b.cc"))
		<< "Found:\n"
		<< sources_path_str(ab_m.sources());

	auto abc_m =
		std::get<ecsact::build_recipe>(ecsact::build_recipe::merge(ab_m, c));

	EXPECT_EQ(abc_m.base_directory(), ab_m.base_directory());

	EXPECT_TRUE(contains_source_path(abc_m.sources(), "z/x/y/z.cc"))
		<< "Found:\n"
		<< sources_path_str(abc_m.sources());

	EXPECT_TRUE(contains_source_path(abc_m.sources(), "../bbb/b.cc"))
		<< "Found:\n"
		<< sources_path_str(abc_m.sources());

	EXPECT_TRUE(contains_source_path(abc_m.sources(), "b/q/x/y/v/d.cc"))
		<< "Found:\n"
		<< sources_path_str(abc_m.sources());
}
