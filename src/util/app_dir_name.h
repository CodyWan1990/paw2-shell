#pragma once

namespace FileUtils {

  // Directory (and socket, log, asset) name for this shell, so it installs and runs alongside other
  // shells without sharing config or state.
  inline constexpr const char* kAppDirName = "paw2-shell";

} // namespace FileUtils
