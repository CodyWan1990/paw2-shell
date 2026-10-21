#pragma once

#include <string>

namespace paw2shell::launcher {

  struct DmenuSocketPathResult {
    std::string path;
    std::string error;
  };

  [[nodiscard]] DmenuSocketPathResult resolveDmenuSocketPath();

} // namespace paw2shell::launcher
