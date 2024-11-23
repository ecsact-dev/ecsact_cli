#include "load_runfiles.hh"

#include <vector>
#include <string>

#include "tools/cpp/runfiles/runfiles.h"

auto ecsact::cli::cook::load_runfiles() -> void {
	using bazel::tools::cpp::runfiles::Runfiles;
	auto runfiles_error = std::string{};
	auto runfiles = std::unique_ptr<Runfiles>(
		Runfiles::Create(argv0, BAZEL_CURRENT_REPOSITORY, &runfiles_error)
	);
	if(!runfiles) {
		ecsact::cli::report_error("Failed to load runfiles: {}", runfiles_error);
		return {};
	}

	ecsact::cli::report_info("Using ecsact headers from runfiles");

	auto ecsact_runtime_headers_from_runfiles = std::vector<std::string>{
		"ecsact_runtime/ecsact/lib.hh",
		"ecsact_runtime/ecsact/runtime.h",
		"ecsact_runtime/ecsact/runtime/async.h",
		"ecsact_runtime/ecsact/runtime/async.hh",
		"ecsact_runtime/ecsact/runtime/common.h",
		"ecsact_runtime/ecsact/runtime/core.h",
		"ecsact_runtime/ecsact/runtime/core.hh",
		"ecsact_runtime/ecsact/runtime/definitions.h",
		"ecsact_runtime/ecsact/runtime/dylib.h",
		"ecsact_runtime/ecsact/runtime/dynamic.h",
		"ecsact_runtime/ecsact/runtime/meta.h",
		"ecsact_runtime/ecsact/runtime/meta.hh",
		"ecsact_runtime/ecsact/runtime/serialize.h",
		"ecsact_runtime/ecsact/runtime/serialize.hh",
		"ecsact_runtime/ecsact/runtime/static.h",
	};

	for(auto hdr : ecsact_runtime_headers_from_runfiles) {
		auto full_hdr_path = runfiles->Rlocation(hdr);

		if(full_hdr_path.empty()) {
			ecsact::cli::report_error(
				"Cannot find ecsact_runtime header in runfiles: {}",
				hdr
			);
			return {};
		}

		auto rel_hdr_path = hdr.substr("ecsact_runtime/"sv.size());
		fs::create_directories((inc_dir / rel_hdr_path).parent_path(), ec);

		if(fs::exists(inc_dir / rel_hdr_path)) {
			ecsact::cli::report_warning(
				"Overwriting ecsact runtime header {} with runfiles header",
				rel_hdr_path
			);
		}

		fs::copy_file(
			full_hdr_path,
			inc_dir / rel_hdr_path,
			fs::copy_options::overwrite_existing,
			ec
		);
		if(ec) {
			ecsact::cli::report_error(
				"Failed to copy ecsact runtime header from runfiles. {} -> {}\n{}",
				full_hdr_path,
				(inc_dir / rel_hdr_path).generic_string(),
				ec.message()
			);
			return {};
		}
	}
}
