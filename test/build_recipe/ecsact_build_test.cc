#include "ecsact/runtime/core.h"

#include "local_dep.hh"

ecsact_execute_systems_error ecsact_execute_systems( //
  ecsact_registry_id,
  int,
  const ecsact_execution_options*,
  const ecsact_execution_events_collector*
) {
	local_dep::example();
	return ECSACT_EXEC_SYS_OK;
}
