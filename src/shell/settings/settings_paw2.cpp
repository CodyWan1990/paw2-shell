#include "shell/settings/settings_paw2.h"

#include "compositors/paw2/paw2_config_writer.h"

#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
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

  void applyPaw2Setting(
      compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path,
      const ConfigOverrideValue& value
  ) {
    nlohmann::json json = toJson(value);
    if (json.is_null()) {
      return;
    }
    if (isMonitorPath(path)) {
      writer.setMonitor(path[2], path[3], std::move(json));
      return;
    }
    if (isBindingPath(path)) {
      writer.setBinding(dottedPaw2Path(path, 2), std::move(json));
      return;
    }
    const std::string dotted = dottedPaw2Path(path);
    // paw2 rejects blank strings ("theme.font cannot be empty"); a cleared field means "not set".
    if (json.is_string() && json.get<std::string>().empty()) {
      writer.unset(dotted);
      return;
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
  }

  void resetPaw2Setting(compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path) {
    if (isMonitorPath(path)) {
      writer.setMonitor(path[2], path[3], nullptr);
    } else if (isBindingPath(path)) {
      writer.setBinding(dottedPaw2Path(path, 2), nullptr);
    } else {
      writer.unset(dottedPaw2Path(path));
    }
  }

  bool paw2SettingOverridden(const compositors::paw2::Paw2ConfigWriter& writer, const std::vector<std::string>& path) {
    // Monitors and shortcuts are written whole, so no single key of theirs has a default to go back to.
    if (isMonitorPath(path) || isBindingPath(path)) {
      return false;
    }
    return writer.owns(dottedPaw2Path(path));
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
