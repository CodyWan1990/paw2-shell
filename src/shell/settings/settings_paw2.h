#pragma once

#include "config/config_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace compositors::paw2 {
  class Paw2ConfigWriter;
}

namespace settings {

  // Rows on the paw2 pages carry paths under "paw2", and are paw2's own settings rather than the
  // shell's: {"paw2", "appearance", "gaps"} is paw2's appearance.gaps, {"paw2", "monitors", "DP-1",
  // "scale"} one monitor's scale, and {"paw2", "bindings", "close_window"} one shortcut.
  [[nodiscard]] bool isPaw2SettingPath(const std::vector<std::string>& path);

  void applyPaw2Setting(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path,
      const ConfigOverrideValue& value
  );
  // Hands the key back to config.toml, or to paw2's compiled default.
  void resetPaw2Setting(compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path);
  // Whether the shell's drop-in sets the key, which is when it can be reset.
  [[nodiscard]] bool
  paw2SettingOverridden(const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path);

  enum class DesktopThemeKind : std::uint8_t {
    Cursor,
    Icon,
    Gtk,
  };
  // Installed theme names, sorted, from the user's and the system's theme directories.
  [[nodiscard]] std::vector<std::string> installedDesktopThemes(DesktopThemeKind kind);

} // namespace settings
