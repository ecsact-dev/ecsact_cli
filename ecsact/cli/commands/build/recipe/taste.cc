#include "ecsact/cli/commands/build/recipe/taste.hh"

#include <format>
#include <algorithm>
#include <memory>
#include <filesystem>
#include <boost/dll.hpp>
#include <boost/dll/library_info.hpp>
#include "ecsact/cli/report.hh"
#include "ecsact/runtime/dylib.h"

namespace fs = std::filesystem;

static auto maybe_library_info( //
	std::filesystem::path library_path
) -> std::unique_ptr<boost::dll::library_info> {
	try {
		return std::unique_ptr<boost::dll::library_info>(
			new boost::dll::library_info{
				library_path.string(),
				false,
			}
		);
	} catch(const std::exception&) {
		return nullptr;
	}
}

auto ecsact::cli::taste_recipe( //
	const ecsact::build_recipe& recipe,
	std::filesystem::path       output_path
) -> int {
	auto ec = std::error_code{};

	auto runtime_lib_info = maybe_library_info(output_path.string());

	if(!runtime_lib_info) {
		ecsact::cli::report_error(
			"Unable to load library info for {}",
			output_path.string()
		);
		return 1;
	}

	auto recipe_exports = recipe.exports();
	auto recipe_imports = recipe.imports();

	auto missing_export_symbols = std::vector<std::string>{};
	missing_export_symbols.reserve(recipe_exports.size());

	for(auto recipe_export : recipe_exports) {
		auto has_export_symbol = false;
		for(auto symbol : runtime_lib_info->symbols()) {
			if(recipe_export == symbol) {
				has_export_symbol = true;
			}
		}

		if(!has_export_symbol) {
			missing_export_symbols.push_back(recipe_export);
		}
	}

	for(auto export_symbol : missing_export_symbols) {
		ecsact::cli::report_error("Missing export symbol '{}'", export_symbol);
	}

	if(!missing_export_symbols.empty()) {
		auto found_symbols = runtime_lib_info->symbols();
		if(found_symbols.empty()) {
			ecsact::cli::report_warning("No symbols found");
		} else {
			for(auto symbol : runtime_lib_info->symbols()) {
				ecsact::cli::report_info("Found symbol: {}", symbol);
			}
		}

		return 1;
	}

	if(!fs::exists(output_path)) {
		ecsact::cli::report_error(
			"Output path {} does not exist",
			output_path.string()
		);
		return 1;
	}

	auto runtime_lib = boost::dll::shared_library{
		output_path.string(),
		boost::dll::load_mode::default_mode,
		ec,
	};

	auto has_dylib_set_fn_symbol = false;
	auto has_dylib_has_fn_symbol = false;

	if(!recipe_imports.empty()) {
		for(auto symbol : runtime_lib_info->symbols()) {
			if(symbol == "ecsact_dylib_has_fn") {
				has_dylib_has_fn_symbol = true;
			} else if(symbol == "ecsact_dylib_set_fn_addr") {
				has_dylib_set_fn_symbol = true;
			}
		}

		if(!has_dylib_set_fn_symbol) {
			ecsact::cli::report_error(
				"Build recipes with imports must export 'ecsact_dylib_set_fn_addr' "
				"(internal error)"
			);
			return 1;
		}
	}

	if(ec) {
		if(recipe_imports.empty()) {
			ecsact::cli::report_warning(
				"Unable to load {}: {}",
				output_path.string(),
				ec.message()
			);
		} else {
			ecsact::cli::report_error(
				"Unable to load {}: {}",
				output_path.string(),
				ec.message()
			);
			ecsact::cli::report_error(
				"Cannot verify recipe imports if runtime does not load"
			);
			return 1;
		}
	} else {
		if(has_dylib_has_fn_symbol) {
			decltype(&ecsact_dylib_has_fn) dylib_has_fn = nullptr;
			dylib_has_fn =
				runtime_lib.get<decltype(ecsact_dylib_has_fn)>("ecsact_dylib_has_fn");

			for(auto imp : recipe_imports) {
				if(!dylib_has_fn(imp.c_str())) {
					ecsact::cli::report_error(
						"ecsact_dylib_has_fn(\"{0}\") returned false",
						imp
					);
					return 1;
				}
			}
		}

		runtime_lib.unload();
	}

	return 0;
}
