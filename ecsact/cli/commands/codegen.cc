#include "./codegen.hh"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "ecsact/interpret/eval.hh"
#include "ecsact/cli/commands/common.hh"
#include "ecsact/cli/report.hh"
#include "ecsact/runtime/dynamic.h"
#include "ecsact/runtime/meta.hh"
#include "ecsact/runtime/dylib.h"
#include "ecsact/codegen/plugin.h"
#include "ecsact/codegen/plugin_validate.hh"
#include "ecsact/cli/commands/codegen/codegen_util.hh"

namespace fs = std::filesystem;
constexpr auto file_readonly_perms = fs::perms::others_read |
	fs::perms::group_read | fs::perms::owner_read;

constexpr auto USAGE = R"(Ecsact Codegen Command

Usage:
  ecsact codegen <files>... --plugin=<plugin> [--stdout]
  ecsact codegen <files>... --plugin=<plugin>... [--outdir=<directory>] [--format=<type>] [--report_filter=<filter>]

Options:
  -p, --plugin=<plugin>     Name of bundled plugin or path to plugin.
  --stdout                  Print to stdout instead of writing to a file. May only be used if a single ecsact file and single ecsact codegen plugin are used.
  -o, --outdir=<directory>  Specify directory generated files should be written to. By default generated files are written next to source files.
  -f --format=<type>        The format used to report progress of the build [default: text]
  --report_filter=<filter>  Filtering out report logs [default: none]
)";

static void stdout_write_fn(const char* str, int32_t str_len) {
	std::cout << std::string_view(str, str_len);
}

static bool received_fatal_codegen_report = false;

static auto codegen_report_fn(
	ecsact_codegen_report_message_type type,
	const char*                        str,
	int32_t                            str_len
) -> void {
	auto msg = std::string{str, static_cast<size_t>(str_len)};
	switch(type) {
		default:
		case ECSACT_CODEGEN_REPORT_INFO:
			return report(ecsact::cli::info_message{msg});
		case ECSACT_CODEGEN_REPORT_WARNING:
			return report(ecsact::cli::warning_message{msg});
		case ECSACT_CODEGEN_REPORT_ERROR:
			return report(ecsact::cli::error_message{msg});
		case ECSACT_CODEGEN_REPORT_FATAL:
			received_fatal_codegen_report = true;
			return report(ecsact::cli::error_message{msg});
	}
}

static thread_local std::ofstream file_write_stream;

static void file_write_fn(const char* str, int32_t str_len) {
	file_write_stream << std::string_view(str, str_len);
}

int ecsact::cli::detail::codegen_command(int argc, const char* argv[]) {
	using namespace std::string_literals;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	if(auto exit_code = process_common_args(args); exit_code != 0) {
		return exit_code;
	}

	auto files_error = false;
	auto files_str = args.at("<files>").asStringList();
	auto files = std::vector<fs::path>{};
	files.reserve(files_str.size());

	for(fs::path file_path : files_str) {
		files.push_back(file_path);
		if(file_path.extension() != ".ecsact") {
			files_error = true;
			report_error(
				"Expected .ecsact file instead got '{}'",
				file_path.string()
			);
		} else if(!fs::exists(file_path)) {
			files_error = true;
			report_error("'{}' does not exist", file_path.string());
		}
	}

	auto default_plugins_dir = get_default_plugins_dir();
	auto plugin_paths = std::vector<fs::path>{};
	auto plugins = std::vector<boost::dll::shared_library>{};
	auto plugins_not_found = false;
	auto invalid_plugins = false;

	for(auto plugin_arg : args.at("--plugin").asStringList()) {
		auto checked_plugin_paths = std::vector<fs::path>{};
		auto plugin_path = resolve_plugin_path(
			{
				.plugin_arg = plugin_arg,
				.default_plugins_dir = default_plugins_dir,
				.additional_plugin_dirs = {},
			},
			checked_plugin_paths
		);

		if(plugin_path) {
			std::error_code ec;
			auto&           plugin = plugins.emplace_back();
			plugin.load(plugin_path->string(), ec);
			auto validate_result = ecsact::codegen::plugin_validate(*plugin_path);
			if(validate_result.ok()) {
				plugin_paths.emplace_back(*plugin_path);
			} else {
				invalid_plugins = true;
				plugins.pop_back();
				auto err_msg = "Plugin validation failed for '" + plugin_arg + "'\n";
				for(auto err : validate_result.errors) {
					err_msg += " - "s + to_string(err) + "\n";
				}
				report_error("{}", err_msg);
			}
		} else {
			plugins_not_found = true;
			auto err_msg =
				"Unable to find codegen plugin '" + plugin_arg + "'. Paths checked:\n";
			for(auto& checked_path : checked_plugin_paths) {
				err_msg += " - " + fs::relative(checked_path).string() + "\n";
			}
			report_error("{}", err_msg);
		}
	}

	if(invalid_plugins || plugins_not_found || files_error) {
		return 1;
	}

	if(auto main_pkg_id = ecsact::meta::main_package(); main_pkg_id) {
		ecsact_destroy_package(*main_pkg_id);
	}
	auto eval_errors = ecsact::eval_files(files);
	if(!eval_errors.empty()) {
		for(auto& eval_err : eval_errors) {
			report_error(
				"{}:{}:{} {}",
				files[eval_err.source_index].string(),
				eval_err.line,
				eval_err.character,
				eval_err.error_message
			);
		}
		return 1;
	}

	std::vector<ecsact_package_id> package_ids;
	package_ids.resize(static_cast<int32_t>(ecsact_meta_count_packages()));
	ecsact_meta_get_package_ids(
		static_cast<int32_t>(package_ids.size()),
		package_ids.data(),
		nullptr
	);

	std::unordered_set<std::string> output_paths;
	bool                            has_plugin_error = false;

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

		std::optional<fs::path> outdir;
		if(args.at("--outdir").isString()) {
			outdir = fs::path(args.at("--outdir").asString());
			if(!fs::exists(*outdir)) {
				std::error_code ec;
				fs::create_directories(*outdir, ec);
				if(ec) {
					report_error(
						"Failed to create out directory {}: {}",
						outdir->string(),
						ec.message()
					);
					return 3;
				}
			}
		}

		if(args.at("--stdout").asBool()) {
			plugin_fn(*package_ids.begin(), &stdout_write_fn, &codegen_report_fn);
			std::cout.flush();
			if(received_fatal_codegen_report) {
				received_fatal_codegen_report = false;
				has_plugin_error = true;
				report_error("Codegen plugin '{}' reported fatal error", plugin_name);
			}
		} else {
			for(auto package_id : package_ids) {
				fs::path output_file_path = ecsact_meta_package_file_path(package_id);
				if(output_file_path.empty()) {
					report_error(
						"Could not find package source file path from "
						"'ecsact_meta_package_file_path'"
					);
					continue;
				}

				output_file_path.replace_extension(
					output_file_path.extension().string() + "." + plugin_name
				);

				if(outdir) {
					output_file_path = *outdir / output_file_path.filename();
				}

				if(output_paths.contains(output_file_path.string())) {
					has_plugin_error = true;
					report_error(
						"Plugin '{}' has conflicts with another plugin output file '{}'",
						plugin.location().filename().string(),
						output_file_path.string()
					);
					continue;
				}

				output_paths.emplace(output_file_path.string());
				if(fs::exists(output_file_path)) {
					fs::permissions(output_file_path, fs::perms::all);
				}
				file_write_stream.open(output_file_path);
				plugin_fn(package_id, &file_write_fn, &codegen_report_fn);
				file_write_stream.flush();
				file_write_stream.close();
				fs::permissions(output_file_path, file_readonly_perms);
				if(received_fatal_codegen_report) {
					received_fatal_codegen_report = false;
					report_error(
						"Codegen plugin '{}' reported fatal error while processing package "
						"'{}'",
						plugin_name,
						ecsact::meta::package_name(package_id)
					);
					has_plugin_error = true;
				}
			}
		}

		plugin.unload();
	}

	if(has_plugin_error) {
		return 2;
	}

	return 0;
}
