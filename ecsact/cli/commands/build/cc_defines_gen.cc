#include "ecsact/cli/commands/build/cc_defines_gen.hh"

#include <ranges>
#include "ecsact/runtime/core.h"
#include "ecsact/runtime/dynamic.h"
#include "ecsact/runtime/meta.h"
#include "ecsact/runtime/serialize.h"

auto ecsact::cli::cc_defines_gen( //
	std::vector<std::string> imports,
	std::vector<std::string> exports
) -> std::vector<std::string> {
	auto minimal_exports = //
		exports |
		std::views::filter( //
			[&](const std::string& exp) -> bool {
				for(auto imp : imports) {
					if(imp == exp) {
						return false;
					}
				}
				return true;
			}
		);

	auto defines = std::vector<std::string>{};


#define CHECK_MODULE(fn, exports, output)\
	output = false;\
	for(auto exp : exports) {\
		if(exp == #fn) { output = true; break; }\
	}

	auto has_core_export = false;
	FOR_EACH_ECSACT_CORE_API_FN(CHECK_MODULE, minimal_exports, has_core_export)
	if(has_core_export) {
		defines.push_back("ECSACT_CORE_API_EXPORT");
	}

	auto has_core_import = false;
	FOR_EACH_ECSACT_CORE_API_FN(CHECK_MODULE, imports, has_core_import)
	if(has_core_import) {
		defines.push_back("ECSACT_CORE_API_LOAD_AT_RUNTIME");
	}

	auto has_dynamic_export = false;
	FOR_EACH_ECSACT_DYNAMIC_API_FN(CHECK_MODULE, minimal_exports, has_dynamic_export)
	if(has_dynamic_export) {
		defines.push_back("ECSACT_DYNAMIC_API_EXPORT");
	}

	auto has_dynamic_import = false;
	FOR_EACH_ECSACT_DYNAMIC_API_FN(CHECK_MODULE, imports, has_dynamic_import)
	if(has_dynamic_import) {
		defines.push_back("ECSACT_DYNAMIC_API_LOAD_AT_RUNTIME");
	}

	auto has_meta_export = false;
	FOR_EACH_ECSACT_META_API_FN(CHECK_MODULE, minimal_exports, has_meta_export)
	if(has_meta_export) {
		defines.push_back("ECSACT_META_API_EXPORT");
	}

	auto has_meta_import = false;
	FOR_EACH_ECSACT_META_API_FN(CHECK_MODULE, imports, has_meta_import)
	if(has_meta_import) {
		defines.push_back("ECSACT_META_API_LOAD_AT_RUNTIME");
	}

	auto has_serialize_export = false;
	FOR_EACH_ECSACT_SERIALIZE_API_FN(CHECK_MODULE, minimal_exports, has_serialize_export)
	if(has_serialize_export) {
		defines.push_back("ECSACT_SERIALIZE_API_EXPORT");
	}

	auto has_serialize_import = false;
	FOR_EACH_ECSACT_SERIALIZE_API_FN(CHECK_MODULE, imports, has_serialize_import)
	if(has_serialize_import) {
		defines.push_back("ECSACT_SERIALIZE_API_LOAD_AT_RUNTIME");
	}

#undef CHECK_MODULE

	return defines;
}
