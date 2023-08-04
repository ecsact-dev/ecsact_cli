#include "ecsact/runtime/core.h"

ecsact_execute_systems_error ecsact_execute_systems( //
  ecsact_registry_id,
  int,
  const ecsact_execution_options*,
  const ecsact_execution_events_collector*
) {
  return ECSACT_EXEC_SYS_OK;
}