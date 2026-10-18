#include "shell/settings/settings_paw2.h"

#include "compositors/paw2/paw2_config_writer.h"
#include "ui/style.h"
#include "util/app_dir_name.h"
#include "util/string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
#include <variant>

namespace settings {
  namespace {

    // {"paw2", "appearance", "gaps"} -> "appearance.gaps"
    [[nodiscard]] std::string dottedPaw2Path(const std::vector<std::string>& path, std::size_t from = 1) {
      std::string dotted;
      for (std::size_t i = from; i < path.size(); ++i) {
        if (!dotted.empty()) {
          dotted.push_back('.');
        }
        dotted += path[i];
      }
      return dotted;
    }

    [[nodiscard]] bool isMonitorPath(const std::vector<std::string>& path) {
      return path.size() == 4 && path[1] == "monitors";
    }

    [[nodiscard]] bool isBindingPath(const std::vector<std::string>& path) {
      return path.size() >= 3 && path[1] == "bindings";
    }

    // {"paw2", "bindings", "commands", "Super+Return"}
    [[nodiscard]] bool isCommandBindingPath(const std::vector<std::string>& path) {
      return path.size() == 4 && path[1] == "bindings" && path[2] == "commands";
    }

    constexpr auto kShellShortcuts = std::to_array<ShellShortcut>({
        {"theme-next", "theme-next", "paw2-keys-themes"},
        {"theme-previous", "theme-previous", "paw2-keys-themes"},
        {"themes-open", "panel-toggle themes", "paw2-keys-themes"},
        {"theme-wallpaper-next", "theme-wallpaper-next", "paw2-keys-themes"},
        {"theme-wallpaper-previous", "theme-wallpaper-previous", "paw2-keys-themes"},
        {"theme-wallpaper-random", "theme-wallpaper-random", "paw2-keys-themes"},
    });

    // {"paw2", "bindings", "shell", "theme-next"}
    [[nodiscard]] const ShellShortcut* shellShortcutAt(const std::vector<std::string>& path) {
      if (path.size() != 4 || path[1] != "bindings" || path[2] != "shell") {
        return nullptr;
      }
      const auto it = std::ranges::find(kShellShortcuts, path[3], &ShellShortcut::name);
      return it != kShellShortcuts.end() ? &*it : nullptr;
    }

    // Unbind every key that runs `shortcut`, leaving other commands alone.
    void unbindShellShortcut(compositors::paw2::Paw2ConfigWriter& writer, const ShellShortcut& shortcut) {
      const auto commands = writer.get("bindings.commands").value_or(nlohmann::json::object());
      for (const auto& key : shellShortcutKeys(commands, shortcut)) {
        writer.setBinding("commands." + key, nullptr);
      }
    }

    constexpr auto kBindingActions = std::to_array<Paw2BindingAction>({
        {"focus_left", "paw2-keys-windows", "Super+h Super+Left"},
        {"focus_right", "paw2-keys-windows", "Super+l Super+Right"},
        {"focus_up", "paw2-keys-windows", "Super+k Super+Up"},
        {"focus_down", "paw2-keys-windows", "Super+j Super+Down"},
        {"swap_left", "paw2-keys-windows", "Super+Shift+h Super+Shift+Left"},
        {"swap_right", "paw2-keys-windows", "Super+Shift+l Super+Shift+Right"},
        {"swap_up", "paw2-keys-windows", "Super+Shift+k Super+Shift+Up"},
        {"swap_down", "paw2-keys-windows", "Super+Shift+j Super+Shift+Down"},
        {"close_window", "paw2-keys-windows", "Super+q Super+BackSpace"},
        {"toggle_floating", "paw2-keys-windows", ""},
        {"minimize_window", "paw2-keys-windows", ""},
        {"restore_minimized", "paw2-keys-windows", ""},
        {"toggle_fullscreen", "paw2-keys-windows", ""},
        {"consume_or_expel_left", "paw2-keys-windows", ""},
        {"consume_or_expel_right", "paw2-keys-windows", ""},
        {"cycle_layout", "paw2-keys-layout", ""},
        {"switch_layout_scrolling", "paw2-keys-layout", ""},
        {"switch_layout_dwindle", "paw2-keys-layout", ""},
        {"switch_layout_master_stack", "paw2-keys-layout", ""},
        {"switch_layout_monocle", "paw2-keys-layout", ""},
        {"cycle_column_width", "paw2-keys-layout", ""},
        {"decrease_column_width", "paw2-keys-layout", ""},
        {"increase_column_width", "paw2-keys-layout", ""},
        {"decrease_row_height", "paw2-keys-layout", ""},
        {"increase_row_height", "paw2-keys-layout", ""},
        {"tab_group_left", "paw2-keys-tabs", ""},
        {"tab_group_right", "paw2-keys-tabs", ""},
        {"previous_tab", "paw2-keys-tabs", ""},
        {"next_tab", "paw2-keys-tabs", ""},
        {"tab_group_drag", "paw2-keys-tabs", ""},
        {"workspace.previous", "paw2-keys-workspaces", ""},
        {"workspace.next", "paw2-keys-workspaces", ""},
        {"workspace.move_previous", "paw2-keys-workspaces", ""},
        {"workspace.move_next", "paw2-keys-workspaces", ""},
        {"focus_monitor_left", "paw2-keys-monitors", ""},
        {"focus_monitor_right", "paw2-keys-monitors", ""},
        {"focus_monitor_up", "paw2-keys-monitors", ""},
        {"focus_monitor_down", "paw2-keys-monitors", ""},
        {"move_to_monitor_left", "paw2-keys-monitors", ""},
        {"move_to_monitor_right", "paw2-keys-monitors", ""},
        {"move_to_monitor_up", "paw2-keys-monitors", ""},
        {"move_to_monitor_down", "paw2-keys-monitors", ""},
        {"toggle_overview", "paw2-keys-session", ""},
        {"toggle_scratchpad_overview", "paw2-keys-session", ""},
        {"quit", "paw2-keys-session", "Super+Shift+Escape"},
    });

    [[nodiscard]] nlohmann::json toJson(const ConfigOverrideValue& value) {
      return std::visit(
          [](const auto& v) -> nlohmann::json {
            using T = std::decay_t<decltype(v)>;
            if constexpr (
                std::is_same_v<T, bool>
                || std::is_same_v<T, std::int64_t>
                || std::is_same_v<T, double>
                || std::is_same_v<T, std::string>
                || std::is_same_v<T, std::vector<std::string>>
            ) {
              return v;
            } else {
              // No paw2 row edits the shell's structured lists.
              return nullptr;
            }
          },
          value
      );
    }

    // "1920, 0", "1920,0" or "1920 0"
    [[nodiscard]] std::optional<nlohmann::json> parsePosition(const std::string& text) {
      std::string spaced = text;
      std::ranges::replace(spaced, ',', ' ');
      std::istringstream in(spaced);
      std::int64_t x = 0;
      std::int64_t y = 0;
      std::string rest;
      if (!(in >> x >> y) || (in >> rest)) {
        return std::nullopt;
      }
      return nlohmann::json::array({x, y});
    }

    // One monitor's row. Most map straight onto a [[monitors]] key; the mode is two keys, and a
    // blank position means paw2 places the monitor itself.
    [[nodiscard]] bool setMonitorRow(
        compositors::paw2::Paw2ConfigWriter& writer, const std::string& connector, const std::string& key,
        nlohmann::json value
    ) {
      if (key == "mode" && value.is_string()) {
        const std::string mode = value.get<std::string>();
        if (mode == kPaw2AutoMode) {
          writer.setMonitor(connector, "resolution", nullptr);
          writer.setMonitor(connector, "refresh", nullptr);
          return true;
        }
        int width = 0;
        int height = 0;
        double refresh = 0.0;
        if (std::sscanf(mode.c_str(), "%dx%d@%lf", &width, &height, &refresh) != 3) {
          return false;
        }
        writer.setMonitor(connector, "resolution", nlohmann::json::array({width, height}));
        writer.setMonitor(connector, "refresh", refresh);
        return true;
      }
      if (key == "position" && value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text.find_first_not_of(" \t") == std::string::npos) {
          writer.setMonitor(connector, "position", nullptr);
          return true;
        }
        const auto position = parsePosition(text);
        if (!position.has_value()) {
          return false;
        }
        writer.setMonitor(connector, "position", *position);
        return true;
      }
      // paw2 lets only one monitor take the focus at startup; choosing one un-chooses the others.
      if (key == "focus_at_startup" && value == true) {
        for (const auto& monitor : writer.get("monitors").value_or(nlohmann::json::array())) {
          const std::string other = monitor.value("connector", std::string{});
          if (other != connector && monitor.value("focus_at_startup", false)) {
            writer.setMonitor(other, "focus_at_startup", false);
          }
        }
      }
      writer.setMonitor(connector, key, std::move(value));
      return true;
    }

    [[nodiscard]] std::optional<std::int64_t>
    integerAt(const compositors::paw2::Paw2ConfigWriter& writer, std::string_view path) {
      const auto value = writer.get(path);
      if (!value.has_value() || !value->is_number()) {
        return std::nullopt;
      }
      return value->get<std::int64_t>();
    }

    void addDataDirs(std::vector<std::filesystem::path>& dirs, std::string_view subdir) {
      const char* home = std::getenv("HOME");
      if (const char* dataHome = std::getenv("XDG_DATA_HOME"); dataHome != nullptr && dataHome[0] != '\0') {
        dirs.emplace_back(std::filesystem::path(dataHome) / subdir);
      } else if (home != nullptr && home[0] != '\0') {
        dirs.emplace_back(std::filesystem::path(home) / ".local/share" / subdir);
      }
      const char* dataDirs = std::getenv("XDG_DATA_DIRS");
      std::stringstream list(dataDirs != nullptr && dataDirs[0] != '\0' ? dataDirs : "/usr/local/share:/usr/share");
      for (std::string dir; std::getline(list, dir, ':');) {
        if (!dir.empty()) {
          dirs.emplace_back(std::filesystem::path(dir) / subdir);
        }
      }
    }

    [[nodiscard]] bool isThemeOfKind(const std::filesystem::path& theme, DesktopThemeKind kind) {
      std::error_code ec;
      switch (kind) {
      case DesktopThemeKind::Cursor:
        return std::filesystem::is_directory(theme / "cursors", ec);
      case DesktopThemeKind::Gtk:
        return std::filesystem::is_directory(theme / "gtk-3.0", ec)
            || std::filesystem::is_directory(theme / "gtk-4.0", ec);
      case DesktopThemeKind::Icon: {
        // Cursor themes carry an index.theme too; an icon theme has icon directories beside it.
        if (!std::filesystem::exists(theme / "index.theme", ec)) {
          return false;
        }
        for (const auto& child : std::filesystem::directory_iterator(theme, ec)) {
          if (child.is_directory(ec) && child.path().filename() != "cursors") {
            return true;
          }
        }
        return false;
      }
      }
      return false;
    }

  } // namespace

  bool isPaw2SettingPath(const std::vector<std::string>& path) { return path.size() >= 2 && path.front() == "paw2"; }

  std::string paw2ModeValue(std::int64_t width, std::int64_t height, double refresh) {
    return std::format("{}x{}@{:.3f}", width, height, refresh);
  }

  bool applyPaw2Setting(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path,
      const ConfigOverrideValue& value
  ) {
    nlohmann::json json = toJson(value);
    if (json.is_null()) {
      return false;
    }
    if (const auto* shortcut = shellShortcutAt(path)) {
      if (!json.is_array()) {
        return false;
      }
      unbindShellShortcut(writer, *shortcut);
      for (const auto& key : json) {
        if (key.is_string() && !key.get<std::string>().empty()) {
          writer.setBinding("commands." + key.get<std::string>(), shellShortcutArgv(*shortcut));
        }
      }
      return true;
    }
    if (isMonitorPath(path)) {
      return setMonitorRow(writer, path[2], path[3], std::move(json));
    }
    if (isCommandBindingPath(path) && json.is_string()) {
      const auto argv = splitCommandLine(json.get<std::string>());
      if (argv.empty()) {
        return false;
      }
      writer.setBinding("commands." + path[3], argv);
      return true;
    }
    if (isBindingPath(path)) {
      writer.setBinding(dottedPaw2Path(path, 2), std::move(json));
      return true;
    }
    const std::string dotted = dottedPaw2Path(path);
    // paw2 rejects blank strings ("theme.font cannot be empty"); a cleared field means "not set".
    if (json.is_string() && json.get<std::string>().empty()) {
      writer.unset(dotted);
      return true;
    }
    // The focus ring sits inside the gap, and paw2 rejects a ring wider than it. Move the other
    // value along rather than refuse the edit.
    if (dotted == "appearance.gaps" && json.is_number()) {
      if (const auto ring = integerAt(writer, "appearance.focus_ring_width");
          ring && *ring > json.get<std::int64_t>()) {
        writer.set("appearance.focus_ring_width", json.get<std::int64_t>());
      }
    } else if (dotted == "appearance.focus_ring_width" && json.is_number()) {
      if (json.get<std::int64_t>() > integerAt(writer, "appearance.gaps").value_or(0)) {
        writer.set("appearance.gaps", json.get<std::int64_t>());
      }
    }
    writer.set(dotted, std::move(json));
    return true;
  }

  void resetPaw2Setting(compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path) {
    if (const auto* shortcut = shellShortcutAt(path)) {
      unbindShellShortcut(writer, *shortcut);
    } else if (isMonitorPath(path)) {
      writer.setMonitor(path[2], path[3], nullptr);
    } else if (isBindingPath(path)) {
      writer.setBinding(dottedPaw2Path(path, 2), nullptr);
    } else {
      writer.unset(dottedPaw2Path(path));
    }
  }

  bool paw2SettingOverridden(const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path) {
    // A monitor entry is written whole, so no single key of it has a default to go back to.
    if (isMonitorPath(path)) {
      return false;
    }
    if (const auto* shortcut = shellShortcutAt(path)) {
      return !shellShortcutKeys(writer.get("bindings.commands").value_or(nlohmann::json::object()), *shortcut).empty();
    }
    // A shortcut set anywhere replaces paw2's default for that action; the commands map has none.
    if (isBindingPath(path)) {
      if (path.size() == 3 && path[2] == "commands") {
        return false;
      }
      const auto value = writer.get("bindings." + dottedPaw2Path(path, 2));
      return value.has_value() && !value->is_null();
    }
    return writer.owns(dottedPaw2Path(path));
  }

  std::vector<std::pair<std::string, nlohmann::json>> linkedPaw2Edits(
      const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& shellPath,
      const ConfigOverrideValue* value
  ) {
    const auto number = [value]() -> std::optional<double> {
      if (const auto* d = std::get_if<double>(value)) {
        return *d;
      }
      if (const auto* i = std::get_if<std::int64_t>(value)) {
        return static_cast<double>(*i);
      }
      return std::nullopt;
    };
    const auto text = [value]() -> std::optional<std::string> {
      if (const auto* s = std::get_if<std::string>(value)) {
        return *s;
      }
      return std::nullopt;
    };
    const auto& path = shellPath;
    std::vector<std::pair<std::string, nlohmann::json>> edits;
    const auto reset = [&edits](std::initializer_list<std::string_view> keys) {
      for (const auto key : keys) {
        edits.emplace_back(key, nullptr);
      }
      return edits;
    };

    if (path == std::vector<std::string>{"shell", "animation", "enabled"}) {
      if (value == nullptr) {
        return reset({"animations.enabled"});
      }
      if (const auto* on = std::get_if<bool>(value)) {
        edits.emplace_back("animations.enabled", *on);
      }
    } else if (path == std::vector<std::string>{"shell", "animation", "speed"}) {
      // The shell divides durations by its speed; paw2 multiplies them by its slowdown.
      if (value == nullptr) {
        return reset({"animations.slowdown"});
      }
      if (const auto speed = number(); speed && *speed > 0.0) {
        edits.emplace_back("animations.slowdown", std::clamp(1.0 / *speed, 0.1, 100.0));
      }
    } else if (path == std::vector<std::string>{"shell", "font_family"}) {
      // paw2 takes family and size in one string; the family changes, the size stays.
      const auto family = value != nullptr ? text() : std::nullopt;
      if (!family || family->empty()) {
        return reset({"theme.font"});
      }
      std::string size = "11";
      if (const auto current = writer.get("theme.font"); current && current->is_string()) {
        const std::string font = current->get<std::string>();
        const auto space = font.find_last_of(' ');
        const std::string last = space == std::string::npos ? std::string{} : font.substr(space + 1);
        if (!last.empty() && std::ranges::all_of(last, [](char ch) {
              return std::isdigit(static_cast<unsigned char>(ch)) || ch == '.';
            })) {
          size = last;
        }
      }
      edits.emplace_back("theme.font", *family + " " + size);
    } else if (path == std::vector<std::string>{"shell", "shadow", "alpha"}) {
      if (value == nullptr) {
        return reset({"appearance.shadow_opacity"});
      }
      if (const auto alpha = number()) {
        edits.emplace_back("appearance.shadow_opacity", std::clamp(*alpha, 0.0, 1.0));
      }
    } else if (path == std::vector<std::string>{"shell", "shadow", "direction"}) {
      if (value == nullptr) {
        return reset({"appearance.shadow_offset_x", "appearance.shadow_offset_y"});
      }
      if (const auto direction = text()) {
        // "down_left" casts the shadow down and to the left, by paw2's default 4 px.
        constexpr int kOffset = 4;
        const auto has = [&direction](std::string_view word) { return direction->find(word) != std::string::npos; };
        edits.emplace_back("appearance.shadow_offset_x", has("left") ? -kOffset : has("right") ? kOffset : 0);
        edits.emplace_back("appearance.shadow_offset_y", has("up") ? -kOffset : has("down") ? kOffset : 0);
      }
    } else if (path == std::vector<std::string>{"shell", "corner_radius_scale"}) {
      if (value == nullptr) {
        return reset({"appearance.corner_radius"});
      }
      if (const auto scale = number()) {
        // Windows round like the shell's large containers.
        edits.emplace_back(
            "appearance.corner_radius", std::clamp(static_cast<int>(std::lround(Style::radiusLg * *scale)), 0, 64)
        );
      }
    }
    return edits;
  }

  void applyLinkedPaw2Edits(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& shellPath,
      const ConfigOverrideValue* value
  ) {
    for (auto& [path, json] : linkedPaw2Edits(writer, shellPath, value)) {
      if (json.is_null()) {
        writer.unset(path);
      } else {
        writer.set(std::move(path), std::move(json));
      }
    }
  }

  std::span<const ShellShortcut> shellShortcuts() { return kShellShortcuts; }

  std::vector<std::string> shellShortcutArgv(const ShellShortcut& shortcut) {
    std::vector<std::string> argv{std::string(FileUtils::kAppDirName), "msg"};
    for (const auto& part : StringUtils::splitWhitespace(shortcut.command)) {
      argv.emplace_back(part);
    }
    return argv;
  }

  std::vector<std::string> shellShortcutKeys(const nlohmann::json& commands, const ShellShortcut& shortcut) {
    std::vector<std::string> keys;
    if (!commands.is_object()) {
      return keys;
    }
    const nlohmann::json argv = shellShortcutArgv(shortcut);
    for (const auto& [key, command] : commands.items()) {
      if (command == argv) {
        keys.push_back(key);
      }
    }
    return keys;
  }

  std::span<const Paw2BindingAction> paw2BindingActions() { return kBindingActions; }

  std::string joinCommandLine(const std::vector<std::string>& argv) {
    std::string line;
    for (const auto& arg : argv) {
      if (!line.empty()) {
        line.push_back(' ');
      }
      const bool plain = !arg.empty() && arg.find_first_of(" \t\"'\\") == std::string::npos;
      if (plain) {
        line += arg;
        continue;
      }
      line.push_back('"');
      for (const char ch : arg) {
        if (ch == '"' || ch == '\\') {
          line.push_back('\\');
        }
        line.push_back(ch);
      }
      line.push_back('"');
    }
    return line;
  }

  std::vector<std::string> splitCommandLine(std::string_view line) {
    std::vector<std::string> argv;
    std::string current;
    bool inArgument = false;
    char quote = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
      const char ch = line[i];
      if (ch == '\\' && i + 1 < line.size() && quote != '\'') {
        current.push_back(line[++i]);
        inArgument = true;
      } else if (quote != 0) {
        if (ch == quote) {
          quote = 0;
        } else {
          current.push_back(ch);
        }
      } else if (ch == '"' || ch == '\'') {
        quote = ch;
        inArgument = true;
      } else if (ch == ' ' || ch == '\t') {
        if (inArgument) {
          argv.push_back(std::exchange(current, {}));
          inArgument = false;
        }
      } else {
        current.push_back(ch);
        inArgument = true;
      }
    }
    if (inArgument) {
      argv.push_back(std::move(current));
    }
    return argv;
  }

  std::vector<std::string> installedDesktopThemes(DesktopThemeKind kind) {
    std::vector<std::filesystem::path> roots;
    const char* home = std::getenv("HOME");
    if (kind == DesktopThemeKind::Gtk) {
      if (home != nullptr && home[0] != '\0') {
        roots.emplace_back(std::filesystem::path(home) / ".themes");
      }
      addDataDirs(roots, "themes");
    } else {
      if (home != nullptr && home[0] != '\0') {
        roots.emplace_back(std::filesystem::path(home) / ".icons");
      }
      addDataDirs(roots, "icons");
    }

    std::set<std::string> names;
    for (const auto& root : roots) {
      std::error_code ec;
      for (const auto& theme : std::filesystem::directory_iterator(root, ec)) {
        if (theme.is_directory(ec) && isThemeOfKind(theme.path(), kind)) {
          names.insert(theme.path().filename().string());
        }
      }
    }
    return {names.begin(), names.end()};
  }

} // namespace settings
