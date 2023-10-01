#pragma once

#include <optional>
#include <filesystem>
#include <string>

namespace ecsact {
  struct cc_compiler {
    std::filesystem::path compiler_path;
    std::string compiler_version;
  };
  
  auto detect_cc_compiler() -> std::optional<cc_compiler>;
}
