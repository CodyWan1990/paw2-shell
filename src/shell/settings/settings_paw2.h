#pragma once

#include "config/config_types.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compositors::paw2 {
  class Paw2ConfigWriter;
}

namespace settings {

  // Rows on the paw2 pages carry paths under "paw2", and are paw2's own settings rather than the
  // shell's: {"paw2", "appearance", "gaps"} is paw2's appearance.gaps, {"paw2", "monitors", "DP-1",
  // "scale"} one monitor's scale, and {"paw2", "bindings", "close_window"} one shortcut.
  [[nodiscard]] bool isPaw2SettingPath(const std::vector<std::string>& path);

  // False when the value can't be put into paw2's terms (a position that isn't "x, y").
  [[nodiscard]] bool applyPaw2Setting(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path,
      const ConfigOverrideValue& value
  );
  // Hands the key back to config.toml, or to paw2's compiled default.
  void resetPaw2Setting(compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path);
  // Whether the shell's drop-in sets the key, which is when it can be reset.
  [[nodiscard]] bool
  paw2SettingOverridden(const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path);

  // The Displays page offers a monitor's modes as one choice, though paw2 stores a mode as two
  // keys, resolution and refresh. A mode's value is "2560x1440@165.080"; "auto" is the preferred one.
  inline constexpr std::string_view kPaw2AutoMode = "auto";
  [[nodiscard]] std::string paw2ModeValue(std::int64_t width, std::int64_t height, double refresh);

  // Shell settings that set paw2's matching ones too, so the Appearance page has one control per
  // idea: Animations, animation speed, font, shadow strength and direction, corner roundness.
  // Returns paw2's dotted paths and values for a shell edit (`value` null: the shell setting was
  // reset, so the paw2 keys are unset too, shown as null), or nothing for an unlinked path.
  [[nodiscard]] std::vector<std::pair<std::string, nlohmann::json>> linkedPaw2Edits(
      const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& shellPath,
      const ConfigOverrideValue* value
  );
  void applyLinkedPaw2Edits(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& shellPath,
      const ConfigOverrideValue* value
  );

  // A compositor action the Shortcuts page binds, as paw2's DEFAULT_BINDINGS (src/config.rs) lists
  // it: its [bindings] key, the page group it sits in, and the keys paw2 uses when the key is
  // absent. paw2 does not send its defaults, so they are mirrored here.
  struct Paw2BindingAction {
    std::string_view key;      // "close_window", or "workspace.previous" for [bindings.workspace]
    std::string_view group;    // e.g. "paw2-keys-windows"
    std::string_view defaults; // space-separated chords; empty means unbound
  };
  [[nodiscard]] std::span<const Paw2BindingAction> paw2BindingActions();

  // A command binding's argv as one line, and back: split on spaces, with "double" or 'single'
  // quotes keeping an argument whole and a backslash escaping the next character.
  [[nodiscard]] std::string joinCommandLine(const std::vector<std::string>& argv);
  [[nodiscard]] std::vector<std::string> splitCommandLine(std::string_view line);

  enum class DesktopThemeKind : std::uint8_t {
    Cursor,
    Icon,
    Gtk,
  };
  // Installed theme names, sorted, from the user's and the system's theme directories.
  [[nodiscard]] std::vector<std::string> installedDesktopThemes(DesktopThemeKind kind);

} // namespace settings
