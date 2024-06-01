#include <gtest/gtest.h>

#include <filesystem>
#include "ecsact/cli/detail/glob.hh"

using ecsact::cli::detail::expand_path_globs;
using ecsact::cli::detail::path_before_glob;
using std::ranges::find;

namespace fs = std::filesystem;

TEST(Glob, SimpleWildcard) {
	ASSERT_NE(std::getenv("BUILD_WORKING_DIRECTORY"), nullptr);
	auto dir = fs::path{std::getenv("BUILD_WORKING_DIRECTORY")};
	auto test_root_dir = dir / "misc" / "glob";
	auto glob_pattern = test_root_dir / "*";
	auto ec = std::error_code{};
	auto paths = expand_path_globs(glob_pattern, ec);
	ASSERT_FALSE(ec);
	auto expected_paths = std::vector{
		test_root_dir / "README.md",
		test_root_dir / "something",
		test_root_dir / "test",
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
	};

	for(auto p : paths) {
		EXPECT_TRUE(find(expected_paths, p) != expected_paths.end())
			<< "Unexpected path: " << p.generic_string() << "\n"
			<< glob_pattern.generic_string() << "\n";
	}
}

TEST(Glob, WildcardWithSuffix) {
	ASSERT_NE(std::getenv("BUILD_WORKING_DIRECTORY"), nullptr);
	auto dir = fs::path{std::getenv("BUILD_WORKING_DIRECTORY")};
	auto test_root_dir = dir / "misc" / "glob";
	auto glob_pattern = test_root_dir / "*.txt";
	auto ec = std::error_code{};
	auto paths = expand_path_globs(glob_pattern, ec);
	ASSERT_FALSE(ec);
	auto expected_paths = std::vector{
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
	};

	for(auto p : paths) {
		EXPECT_TRUE(find(expected_paths, p) != expected_paths.end())
			<< "Unexpected path: " << p.generic_string() << "\n"
			<< "Glob pattern: " << glob_pattern.generic_string() << "\n";
	}
}

TEST(Glob, SimpleRecursiveWildcard) {
	ASSERT_NE(std::getenv("BUILD_WORKING_DIRECTORY"), nullptr);
	auto dir = fs::path{std::getenv("BUILD_WORKING_DIRECTORY")};
	auto test_root_dir = dir / "misc" / "glob";
	auto glob_pattern = test_root_dir / "**";
	auto ec = std::error_code{};
	auto paths = expand_path_globs(glob_pattern, ec);
	ASSERT_FALSE(ec);
	auto expected_paths = std::vector{
		test_root_dir / "README.md",
		test_root_dir / "something",
		test_root_dir / "test",
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
		test_root_dir / "README.md",
		test_root_dir / "something",
		test_root_dir / "something" / "other.txt",
		test_root_dir / "test",
		test_root_dir / "test" / "cant_believe.butter",
		test_root_dir / "test" / "can_believe.nothing",
		test_root_dir / "test" / "wow",
		test_root_dir / "test" / "wow" / "so",
		test_root_dir / "test" / "wow" / "so" / "cool.beans",
		test_root_dir / "test" / "wow" / "wow-something.txt",
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
	};

	for(auto p : paths) {
		EXPECT_TRUE(find(expected_paths, p) != expected_paths.end())
			<< "Unexpected path: " << p.generic_string() << "\n"
			<< glob_pattern.generic_string() << "\n";
	}
}

TEST(Glob, RecursiveWildcardSuffix) {
	ASSERT_NE(std::getenv("BUILD_WORKING_DIRECTORY"), nullptr);
	auto dir = fs::path{std::getenv("BUILD_WORKING_DIRECTORY")};
	auto test_root_dir = dir / "misc" / "glob";
	auto glob_pattern = test_root_dir / "**.txt";
	auto ec = std::error_code{};
	auto paths = expand_path_globs(glob_pattern, ec);
	ASSERT_FALSE(ec);
	auto expected_paths = std::vector{
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
		test_root_dir / "something" / "other.txt",
		test_root_dir / "test" / "wow" / "wow-something.txt",
		test_root_dir / "test.txt",
		test_root_dir / "test2.txt",
	};

	for(auto p : paths) {
		EXPECT_TRUE(find(expected_paths, p) != expected_paths.end())
			<< "Unexpected path: " << p.generic_string() << "\n"
			<< glob_pattern.generic_string() << "\n";
	}
}

TEST(Glob, PathBeforeGlob) {
	ASSERT_EQ(path_before_glob("a/b/*").generic_string(), "a/b");
	ASSERT_EQ(path_before_glob("a/b/*.txt/c/*.d").generic_string(), "a/b");
	ASSERT_EQ(path_before_glob("a/b/c.txt").generic_string(), "a/b/c.txt");
}
