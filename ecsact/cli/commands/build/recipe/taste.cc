#include "ecsact/cli/commands/build/recipe/taste.hh"

#include <format>
#include <algorithm>
#include <boost/dll.hpp>
#include <boost/dll/library_info.hpp>
#include "ecsact/cli/report.hh"

auto ecsact::cli::taste_recipe( //
	const ecsact::build_recipe& recipe,
	std::filesystem::path       output_path
) -> int {
	auto ec = std::error_code{};

	auto runtime_lib_info = boost::dll::library_info{
		output_path.string(),
		false,
	};

	auto runtime_lib = boost::dll::shared_library{
		output_path.string(),
		boost::dll::load_mode::default_mode,
		ec,
	};

	if(ec) {
		ecsact::cli::report_error("Unable to load runtime: {}", ec.message());
		return 1;
	}

	auto recipe_exports = recipe.exports();
	auto recipe_imports = recipe.imports();

	auto missing_export_symbols = std::vector<std::string>{};
	missing_export_symbols.reserve(recipe_exports.size());
	auto missing_import_symbols = std::vector<std::string>{};
	missing_import_symbols.reserve(recipe_imports.size());

	for(auto recipe_export : recipe_exports) {
		auto has_export_symbol = false;
		for(auto symbol : runtime_lib_info.symbols()) {
			if(recipe_export == symbol) {
				has_export_symbol = true;
			}
		}

		if(!has_export_symbol) {
			missing_export_symbols.push_back(recipe_export);
		}
	}

	for(auto recipe_import : recipe_imports) {
		auto has_import_symbol = false;
		for(auto symbol : runtime_lib_info.symbols()) {
			if(recipe_import == symbol) {
				has_import_symbol = true;
			}
		}

		if(!has_import_symbol) {
			missing_import_symbols.push_back(recipe_import);
		}
	}

	for(auto export_symbol : missing_export_symbols) {
		ecsact::cli::report_error("Missing export symbol '{}'", export_symbol);
	}
	for(auto import_symbol : missing_import_symbols) {
		ecsact::cli::report_error("Missing import symbol '{}'", import_symbol);
	}

	if(!missing_export_symbols.empty() || !missing_import_symbols.empty()) {
		runtime_lib.unload();
		return 1;
	}

	runtime_lib.unload();
	return 0;
}
