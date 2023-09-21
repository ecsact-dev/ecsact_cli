#include <string_view>
#include "ecsact/codegen/plugin.h"
#include "ecsact/runtime/dylib.h"

using namespace std::string_view_literals;

auto ecsact_codegen_plugin_name() -> const char* {
	return "txt";
}

auto ecsact_codegen_plugin( //
	ecsact_package_id         package_id,
	ecsact_codegen_write_fn_t write_fn
) -> void {
	auto test_message = "this is just a test, breathe!"sv;
	write_fn(test_message.data(), test_message.size());
}
