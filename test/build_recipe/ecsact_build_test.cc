#include "ecsact/runtime/core.h"
#include "ecsact/runtime/dynamic.h"

#include "local_dep.hh"

ecsact_execute_systems_error ecsact_execute_systems( //
  ecsact_registry_id,
  int,
  const ecsact_execution_options*,
  const ecsact_execution_events_collector*
) {
	local_dep::example();
	// This is an incorrect way to use this function, but this test is only caring
	// about the import
	ecsact_system_execution_context_get(nullptr, {}, nullptr);
	return ECSACT_EXEC_SYS_OK;
}
