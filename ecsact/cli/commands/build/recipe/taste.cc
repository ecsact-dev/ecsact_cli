#include "ecsact/cli/commands/build/recipe/taste.hh"

#include <format>
#include <boost/dll.hpp>
#include "ecsact/cli/report.hh"

auto ecsact::cli::taste_recipe( //
	const ecsact::build_recipe& recipe,
	std::filesystem::path       output_path
) -> int {
	auto ec = std::error_code{};
	auto runtime_lib = boost::dll::shared_library{
		output_path.string(),
		boost::dll::load_mode::default_mode,
		ec,
	};

	if(ec) {
		ecsact::cli::report_error(
			"Unable to load runtime: {}",
			ec.message()
		);
		return 1;
	}

	return 0;
}
