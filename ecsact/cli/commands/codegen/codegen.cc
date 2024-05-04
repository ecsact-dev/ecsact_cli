#include "ecsact/cli/commands/codegen/codegen.hh"

#include <unordered_set>
#include <format>
#include <boost/dll.hpp>
#include "ecsact/cli/report.hh"
#include "ecsact/runtime/meta.hh"
#include "ecsact/codegen/plugin.h"
#include "ecsact/cli/commands/codegen/codegen_util.hh"
#include "ecsact/runtime/dylib.h"
#include "ecsact/cli/detail/executable_path/executable_path.hh"

namespace fs = std::filesystem;
using namespace std::string_literals;

auto ecsact::cli::codegen(codegen_options options) -> int {
	auto plugins = std::vector<boost::dll::shared_library>{};

	auto unload_plugins = [&plugins] {
		for(auto& plugin : plugins) {
			if(plugin) {
				plugin.unload();
			}
		}
	};

	for(auto plugin_path : options.plugin_paths) {
		auto  ec = std::error_code{};
		auto& plugin = plugins.emplace_back();
		plugin.load(plugin_path.string(), ec);
		if(ec) {
			ecsact::cli::report_error(
				"Failed to load plugin {}: {}",
				plugin_path.string(),
				ec.message()
			);
			unload_plugins();
			return 1;
		}
	}

	std::vector<ecsact_package_id> package_ids;
	package_ids.resize(static_cast<int32_t>(ecsact_meta_count_packages()));
	ecsact_meta_get_package_ids(
		static_cast<int32_t>(package_ids.size()),
		package_ids.data(),
		nullptr
	);

	auto output_paths = std::unordered_set<std::string>{};
	auto has_plugin_error = false;

	for(auto& plugin : plugins) {
		// precondition: these methods should've been checked in validation
		assert(plugin.has("ecsact_codegen_plugin"));
		assert(plugin.has("ecsact_codegen_plugin_name"));
		assert(plugin.has("ecsact_dylib_set_fn_addr"));

		auto plugin_fn =
			plugin.get<decltype(ecsact_codegen_plugin)>("ecsact_codegen_plugin");
		auto plugin_name_fn = plugin.get<decltype(ecsact_codegen_plugin_name)>(
			"ecsact_codegen_plugin_name"
		);
		std::string plugin_name = plugin_name_fn();

		decltype(&ecsact_dylib_has_fn) dylib_has_fn = nullptr;

		if(plugin.has("ecsact_dylib_has_fn")) {
			dylib_has_fn =
				plugin.get<decltype(ecsact_dylib_has_fn)>("ecsact_dylib_has_fn");
		}

		auto dylib_set_fn_addr =
			plugin.get<decltype(ecsact_dylib_set_fn_addr)>("ecsact_dylib_set_fn_addr"
			);

		auto set_meta_fn_ptr = [&](const char* fn_name, auto fn_ptr) {
			if(dylib_has_fn && !dylib_has_fn(fn_name)) {
				return;
			}
			dylib_set_fn_addr(fn_name, reinterpret_cast<void (*)()>(fn_ptr));
		};

#define CALL_SET_META_FN_PTR(fn_name, unused) \
	set_meta_fn_ptr(#fn_name, &::fn_name)
		FOR_EACH_ECSACT_META_API_FN(CALL_SET_META_FN_PTR);
#undef CALL_SET_META_FN_PTR

		if(!fs::exists(options.outdir)) {
			auto ec = std::error_code{};
			fs::create_directories(options.outdir, ec);
			if(ec) {
				ecsact::cli::report_error(
					"Failed to create out directory {}: {}",
					options.outdir.string(),
					ec.message()
				);
				unload_plugins();
				return 3;
			}
		}

		for(auto package_id : package_ids) {
			plugin_fn(package_id, options.output_write_fn);
		}

		plugin.unload();
	}
	if(has_plugin_error) {
		return 2;
	}

	return 0;
}
