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
      {"monitors", {{{"connector", "DP-1"}, {"resolution", {2560, 1440}}, {"scale", 1.25}}}},
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

  compositors::paw2::Paw2Output display{.name = "DP-1"};
  display.modes = {
      {.width = 2560, .height = 1440, .refresh = 165.08, .current = true},
      {.width = 2560, .height = 1440, .refresh = 60.0},
      {.width = 1920, .height = 1080, .refresh = 60.0}
  };
  env.paw2Outputs = {display};

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

  // Displays: one group per monitor; a resolution without a refresh is its fastest mode.
  const auto* mode = findEntry(entries, {"paw2", "monitors", "DP-1", "mode"});
  ok &= check(mode != nullptr && mode->group == "paw2-output:DP-1", "a group per monitor");
  if (mode != nullptr) {
    const auto& select = std::get<settings::SelectSetting>(mode->control);
    ok &= check(select.options.size() == 4, "preferred plus each mode");
    ok &= check(select.selectedValue == settings::paw2ModeValue(2560, 1440, 165.08), "configured mode selected");
    ok &= check(select.options[1].label == "2560 × 1440 @ 165.08 Hz", "mode label");
  }
  const auto* scale = findEntry(entries, {"paw2", "monitors", "DP-1", "scale"});
  ok &= check(scale != nullptr && std::get<settings::SliderSetting>(scale->control).value == 1.25, "monitor scale");

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
    (void)settings::applyPaw2Setting(writer, {"paw2", "appearance", "focus_ring_width"}, std::int64_t{4});
    ok &= check(writer.get("appearance.gaps") == nlohmann::json(4), "a wider ring widens the gaps");
    (void)settings::applyPaw2Setting(writer, {"paw2", "appearance", "gaps"}, std::int64_t{2});
    ok &= check(writer.get("appearance.focus_ring_width") == nlohmann::json(2), "narrower gaps narrow the ring");
    (void)settings::applyPaw2Setting(writer, {"paw2", "theme", "font"}, std::string{});
    ok &= check(!writer.get("theme.font").has_value(), "blank text is not written");
    (void)settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "scale"}, 1.5);
    ok &= check(writer.get("monitors")->at(0).at("scale") == 1.5, "monitor rows go to [[monitors]]");
    ok &= check(
        settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "mode"}, std::string("1920x1080@60.000")),
        "mode accepted"
    );
    const auto dp1 = writer.get("monitors")->at(0);
    ok &= check(
        dp1.at("resolution") == nlohmann::json::array({1920, 1080}) && dp1.at("refresh") == 60.0,
        "a mode is resolution and refresh"
    );
    (void)settings::applyPaw2Setting(
        writer, {"paw2", "monitors", "DP-1", "mode"}, std::string(settings::kPaw2AutoMode)
    );
    ok &= check(!writer.get("monitors")->at(0).contains("resolution"), "preferred mode clears both");
    ok &= check(
        settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "position"}, std::string("1920, 0")),
        "position accepted"
    );
    ok &= check(writer.get("monitors")->at(0).at("position") == nlohmann::json::array({1920, 0}), "position");
    ok &= check(
        !settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "position"}, std::string("left")),
        "nonsense position refused"
    );
    (void)settings::applyPaw2Setting(writer, {"paw2", "monitors", "DP-1", "focus_at_startup"}, true);
    (void)settings::applyPaw2Setting(writer, {"paw2", "monitors", "HDMI-A-1", "focus_at_startup"}, true);
    ok &= check(writer.get("monitors")->at(0).at("focus_at_startup") == false, "only one monitor takes focus");
    (void)settings::applyPaw2Setting(writer, {"paw2", "bindings", "close_window"}, std::vector<std::string>{"Super+q"});
    ok &= check(writer.get("bindings.close_window") == nlohmann::json::array({"Super+q"}), "shortcut rows");
  }
  std::filesystem::remove_all(configDir);

  return ok ? 0 : 1;
}
