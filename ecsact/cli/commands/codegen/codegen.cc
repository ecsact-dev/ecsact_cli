#include "ecsact/cli/commands/codegen/codegen.hh"

#include <unordered_set>
#include <boost/dll.hpp>
#include "ecsact/cli/report.hh"
#include "ecsact/runtime/meta.hh"
#include "ecsact/codegen/plugin.h"
#include "ecsact/runtime/dylib.h"
#include "ecsact/cli/detail/executable_path/executable_path.hh"

namespace fs = std::filesystem;

auto ecsact::cli::get_default_plugins_dir() -> fs::path {
	using executable_path::executable_path;

	return fs::weakly_canonical(
		executable_path().parent_path() / ".." / "share" / "ecsact" / "plugins"
	);
}

auto ecsact::cli::current_platform_codegen_plugin_extension() -> std::string {
	return boost::dll::shared_library::suffix().string();
}

auto ecsact::cli::resolve_plugin_path(
	const resolve_plugin_path_options& options,
	std::vector<fs::path>&             checked_plugin_paths
) -> std::optional<fs::path> {
	using namespace std::string_literals;

	const bool is_maybe_named_plugin = options.plugin_arg.find('/') ==
			std::string::npos &&
		options.plugin_arg.find('\\') == std::string::npos &&
		options.plugin_arg.find('.') == std::string::npos;

	if(is_maybe_named_plugin) {
		fs::path& default_plugin_path = checked_plugin_paths.emplace_back(
			options.default_plugins_dir /
			("ecsact_"s + options.plugin_arg + "_codegen"s)
		);

		default_plugin_path.replace_extension(
			current_platform_codegen_plugin_extension()
		);

		if(fs::exists(default_plugin_path)) {
			return default_plugin_path;
		}
	}

	fs::path plugin_path = fs::weakly_canonical(options.plugin_arg);
	if(plugin_path.extension().empty()) {
		plugin_path.replace_extension(current_platform_codegen_plugin_extension());
	}

	if(fs::exists(plugin_path)) {
		return plugin_path;
	}

	if(plugin_path.is_relative()) {
		for(auto dir : options.additional_plugin_dirs) {
			if(fs::exists(dir / plugin_path)) {
				return dir / plugin_path;
			}
		}
	}

	checked_plugin_paths.emplace_back(plugin_path);

	return {};
}

auto ecsact::cli::resolve_plugin_path( //
	const resolve_plugin_path_options& options
) -> std::optional<fs::path> {
	auto checked_plugin_paths = std::vector<fs::path>{};
	auto plugin_path = resolve_plugin_path(options, checked_plugin_paths);

	if(!plugin_path) {
		auto paths_checked_str = std::string{};

		for(auto& checked_path : checked_plugin_paths) {
			paths_checked_str += std::format( //
				" - {}\n",
				fs::relative(checked_path).string()
			);
		}
		ecsact::cli::report_error(
			"Unable to find codegen plugin '{}'. Paths checked:\n{}",
			options.plugin_arg,
			paths_checked_str
		);

		return std::nullopt;
	}

	return plugin_path;
}

auto ecsact::cli::codegen(codegen_options options) -> int {
	using namespace std::string_literals;

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
