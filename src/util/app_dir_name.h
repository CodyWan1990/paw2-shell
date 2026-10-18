#pragma once

namespace FileUtils {

  // Directory (and socket, log, asset) name for this shell. Not "noctalia", so it installs and runs alongside
  // upstream noctalia or another fork without sharing config or state.
  inline constexpr const char* kAppDirName = "paw2-shell";

} // namespace FileUtils
