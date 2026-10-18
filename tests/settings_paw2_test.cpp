#include "compositors/paw2/paw2_config_writer.h"
#include "compositors/paw2/paw2_runtime.h"
#include "config/config_types.h"
#include "i18n/i18n_service.h"
#include "shell/settings/settings_paw2.h"
#include "shell/settings/settings_registry.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

namespace {

  bool check(bool cond, const std::string& msg) {
    if (!cond) {
      std::cerr << "FAIL: " << msg << '\n';
    }
    return cond;
  }

  const settings::SettingEntry*
  findEntry(const std::vector<settings::SettingEntry>& entries, const std::vector<std::string>& path) {
    for (const auto& entry : entries) {
      if (entry.path == path) {
        return &entry;
      }
    }
    return nullptr;
  }

  [[nodiscard]] bool isPaw2Section(settings::SettingsSection section) {
    return section == settings::SettingsSection::Paw2
        || section == settings::SettingsSection::Paw2Displays
        || section == settings::SettingsSection::Paw2Shortcuts;
  }

} // namespace

int main() {
  using compositors::paw2::splitPath;
  bool ok = true;
  i18n::Service::instance().init("en");
  const Config cfg{};

  // paw2's config message carries only what its files set.
  const nlohmann::json paw2Config = {
      {"appearance", {{"gaps", 15}, {"focus_ring_width", 1}}},
      {"layout", {{"enabled_layouts", {"scrolling", "dwindle"}}}},
  };
  settings::RegistryEnvironment env;
  env.paw2Value = [&paw2Config](std::string_view path) -> std::optional<nlohmann::json> {
    const nlohmann::json* node = &paw2Config;
    for (const auto& part : splitPath(path)) {
      if (!node->is_object() || !node->contains(part)) {
        return std::nullopt;
      }
      node = &(*node)[part];
    }
    return *node;
  };

  const auto entries = settings::buildSettingsRegistry(cfg, nullptr, nullptr, env);
  std::size_t paw2Rows = 0;
  for (const auto& entry : entries) {
    if (!isPaw2Section(entry.section)) {
      continue;
    }
    ++paw2Rows;
    ok &= check(!entry.title.starts_with("!!") && !entry.subtitle.starts_with("!!"), "text for " + entry.title);
  }
  ok &= check(paw2Rows > 20, "compositor page has its rows");

  const auto* gaps = findEntry(entries, {"paw2", "appearance", "gaps"});
  ok &= check(gaps != nullptr && std::get<settings::SliderSetting>(gaps->control).value == 15.0, "gaps from paw2");
  const auto* corners = findEntry(entries, {"paw2", "appearance", "corner_radius"});
  ok &= check(
      corners != nullptr && std::get<settings::SliderSetting>(corners->control).value == 0.0,
      "unset keys read as paw2's defaults"
  );
  const auto* layouts = findEntry(entries, {"paw2", "layout", "enabled_layouts"});
  ok &= check(layouts != nullptr && std::get<settings::ListSetting>(layouts->control).items.size() == 2, "layouts");
  const auto* blurRadius = findEntry(entries, {"paw2", "appearance", "blur_radius"});
  ok &= check(
      blurRadius != nullptr && blurRadius->visibleWhen && !blurRadius->visibleWhen(cfg),
      "blur strength hidden while blur is off"
  );

  // Without paw2's config every paw2 page shows why, instead of controls.
  const auto offline = settings::buildSettingsRegistry(cfg, nullptr, nullptr, {});
  std::size_t statusRows = 0;
  for (const auto& entry : offline) {
    if (isPaw2Section(entry.section)) {
      ++statusRows;
      ok &= check(std::holds_alternative<settings::ButtonSetting>(entry.control), "offline row is the notice");
    }
  }
  ok &= check(statusRows == 3, "one notice per paw2 page");

  // Routing: the ring and the gap it sits in move together, and a cleared text field means unset.
  const auto configDir = std::filesystem::temp_directory_path() / ("settings-paw2-test-" + std::to_string(getpid()));
  ::setenv("PAW2_SOCKET", (configDir / "none.sock").c_str(), 1);
  {
    compositors::paw2::Paw2Runtime runtime;
    compositors::paw2::Paw2ConfigWriter writer(runtime, configDir);
    settings::applyPaw2Setting(writer, {"paw2", "appearance", "focus_ring_width"}, std::int64_t{4});
    ok &= check(writer.get("appearance.gaps") == nlohmann::json(4), "a wider ring widens the gaps");
    settings::applyPaw2Setting(writer, {"paw2", "appearance", "gaps"}, std::int64_t{2});
    ok &= check(writer.get("appearance.focus_ring_width") == nlohmann::json(2), "narrower gaps narrow the ring");
    settings::applyPaw2Setting(writer, {"paw2", "theme", "font"}, std::string{});
    ok &= check(!writer.get("theme.font").has_value(), "blank text is not written");
    settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "scale"}, 1.5);
    ok &= check(writer.get("monitors")->at(0).at("scale") == 1.5, "monitor rows go to [[monitors]]");
    settings::applyPaw2Setting(writer, {"paw2", "bindings", "close_window"}, std::vector<std::string>{"Super+q"});
    ok &= check(writer.get("bindings.close_window") == nlohmann::json::array({"Super+q"}), "shortcut rows");
  }
  std::filesystem::remove_all(configDir);

  return ok ? 0 : 1;
}
