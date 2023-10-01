#pragma once

#include <optional>

namespace ecsact {
  struct cc_compiler {
    
  };
  
  auto detect_cc_compiler() -> std::optional<cc_compiler>;
}