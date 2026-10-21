#include "config/widget_config.h"
#include "core/toml.h"

#include <print>
#include <string>
#include <variant>

namespace {

  bool expect(bool condition, const char* message) {
    if (!condition) {
      std::println(stderr, "config_widget_test: FAIL: {}", message);
      return false;
    }
    return true;
  }

  bool expectStringSetting(const WidgetConfig& widget, const std::string& key, const std::string& expected) {
    const auto it = widget.settings.find(key);
    if (it == widget.settings.end()) {
      std::println(stderr, "config_widget_test: FAIL: missing setting '{}'", key);
      return false;
    }
    const auto* actual = std::get_if<std::string>(&it->second);
    if (actual == nullptr || *actual != expected) {
      std::println(stderr, "config_widget_test: FAIL: setting '{}': expected '{}'", key, expected);
      return false;
    }
    return true;
  }

  bool expectBoolSetting(const WidgetConfig& widget, const std::string& key, bool expected) {
    const auto it = widget.settings.find(key);
    if (it == widget.settings.end()) {
      std::println(stderr, "config_widget_test: FAIL: missing setting '{}'", key);
      return false;
    }
    const auto* actual = std::get_if<bool>(&it->second);
    if (actual == nullptr || *actual != expected) {
      std::println(stderr, "config_widget_test: FAIL: setting '{}': expected {}", key, expected);
      return false;
    }
    return true;
  }

  bool expectStringMapEntry(
      const WidgetConfig& widget, const std::string& tableKey, const std::string& key, const std::string& expected
  ) {
    const auto tableIt = widget.tables.find(tableKey);
    if (tableIt == widget.tables.end()) {
      std::println(stderr, "config_widget_test: FAIL: missing table '{}'", tableKey);
      return false;
    }
    const auto valueIt = tableIt->second.find(key);
    if (valueIt == tableIt->second.end() || valueIt->second != expected) {
      std::println(stderr, "config_widget_test: FAIL: table '{}.{}': expected '{}'", tableKey, key, expected);
      return false;
    }
    return true;
  }

} // namespace

int main() {
  bool ok = true;

  Config base;
  paw2shell::config::seedBuiltinWidgets(base);

  const auto parsed = toml::parse("[widget.temp]\nshow_label = false\n");
  const auto* widgetRoot = parsed["widget"].as_table();
  const auto* tempTable = widgetRoot != nullptr ? (*widgetRoot)["temp"].as_table() : nullptr;
  if (!expect(tempTable != nullptr, "parsed widget.temp table")) {
    return 1;
  }

  const WidgetConfig temp = paw2shell::config::readBarWidgetConfig("temp", *tempTable, base);
  ok = expect(temp.type == "sysmon", "temp resolves to sysmon") && ok;
  ok = expectStringSetting(temp, "stat", "cpu_temp") && ok;
  ok = expectBoolSetting(temp, "show_label", false) && ok;

  const auto customParsed = toml::parse("[widget.my_clock]\nformat = \"{:%H:%M}\"\n");
  const auto* customRoot = customParsed["widget"].as_table();
  const auto* customTable = customRoot != nullptr ? (*customRoot)["my_clock"].as_table() : nullptr;
  if (!expect(customTable != nullptr, "parsed widget.my_clock table")) {
    return 1;
  }

  const WidgetConfig custom = paw2shell::config::readBarWidgetConfig("my_clock", *customTable, base);
  ok = expect(custom.type == "my_clock", "unknown widget name resolves to its own type") && ok;
  ok = expectStringSetting(custom, "format", "{:%H:%M}") && ok;

  const auto layoutParsed = toml::parse(
      "[widget.keyboard_layout]\n"
      "show_label = true\n"
      "[widget.keyboard_layout.custom_labels]\n"
      "\"English (US)\" = \"EN\"\n"
  );
  const auto* layoutRoot = layoutParsed["widget"].as_table();
  const auto* layoutTable = layoutRoot != nullptr ? (*layoutRoot)["keyboard_layout"].as_table() : nullptr;
  if (!expect(layoutTable != nullptr, "parsed widget.keyboard_layout table")) {
    return 1;
  }

  const WidgetConfig layout = paw2shell::config::readBarWidgetConfig("keyboard_layout", *layoutTable, base);
  ok = expect(layout.type == "keyboard_layout", "keyboard_layout keeps builtin type") && ok;
  ok = expectBoolSetting(layout, "hide_when_single_layout", false) && ok;
  ok = expectStringMapEntry(layout, "custom_labels", "English (US)", "EN") && ok;

  const auto mapValueParsed = toml::parse("[output_glyphs]\n\"eDP-1\" = \"laptop\"\n\"DP-1\" = \"monitor\"\n");
  const auto* mapValueNode = mapValueParsed.get("output_glyphs");
  if (!expect(mapValueNode != nullptr, "parsed string-map widget setting")) {
    return 1;
  }
  const auto mapValue = paw2shell::config::readWidgetSettingValue(*mapValueNode);
  ok = expect(mapValue.has_value(), "string-map widget setting parses") && ok;
  if (mapValue.has_value()) {
    const auto* values = std::get_if<WidgetSettingStringMap>(&*mapValue);
    ok = expect(values != nullptr, "string-map widget setting has map type") && ok;
    if (values != nullptr) {
      ok = expect(values->size() == 2, "string-map widget setting size") && ok;
      ok = expect(values->at("eDP-1") == "laptop", "string-map widget setting first value") && ok;
      ok = expect(values->at("DP-1") == "monitor", "string-map widget setting second value") && ok;
    }
  }

  const auto invalidMapValueParsed = toml::parse("[output_glyphs]\n\"eDP-1\" = 1\n");
  const auto* invalidMapValueNode = invalidMapValueParsed.get("output_glyphs");
  if (!expect(invalidMapValueNode != nullptr, "parsed invalid string-map widget setting")) {
    return 1;
  }
  ok = expect(
           !paw2shell::config::readWidgetSettingValue(*invalidMapValueNode).has_value(),
           "string-map widget setting rejects non-string values"
       )
      && ok;

  BarConfig bar;
  bar.widgetCapsuleRadius = 12.0;
  WidgetConfig launcher;
  launcher.settings["capsule"] = true;
  launcher.settings["capsule_radius"] = std::string("auto");
  const WidgetBarCapsuleSpec automaticCapsule = resolveWidgetBarCapsuleSpec(bar, &launcher);
  ok = expect(automaticCapsule.enabled, "launcher capsule is enabled with an automatic radius") && ok;
  ok = expect(
           automaticCapsule.radius.has_value() && *automaticCapsule.radius == 12.0F,
           "automatic widget capsule radius keeps the bar radius"
       )
      && ok;

  launcher.settings["capsule_radius"] = 7.0;
  const WidgetBarCapsuleSpec explicitCapsule = resolveWidgetBarCapsuleSpec(bar, &launcher);
  ok = expect(
           explicitCapsule.radius.has_value() && *explicitCapsule.radius == 7.0F,
           "numeric widget capsule radius overrides the bar radius"
       )
      && ok;

  bar.widgetCapsuleBorderWidth = 3.0F;
  const WidgetBarCapsuleSpec inheritedBorderWidth = resolveWidgetBarCapsuleSpec(bar, &launcher);
  ok = expect(inheritedBorderWidth.borderWidth == 3.0F, "widget capsule inherits the bar border width") && ok;

  launcher.settings["capsule_border_width"] = 1.5;
  const WidgetBarCapsuleSpec explicitBorderWidth = resolveWidgetBarCapsuleSpec(bar, &launcher);
  ok = expect(explicitBorderWidth.borderWidth == 1.5F, "widget capsule border width overrides the bar value") && ok;

  launcher.settings["capsule_border_width"] = 99.0;
  const WidgetBarCapsuleSpec clampedBorderWidth = resolveWidgetBarCapsuleSpec(bar, &launcher);
  ok = expect(clampedBorderWidth.borderWidth == 8.0F, "widget capsule border width is clamped to 8") && ok;

  BarCapsuleGroupStyle group;
  group.id = "g1";
  group.borderSpecified = true;
  group.border = colorSpecFromRole(ColorRole::Outline);
  group.borderWidth = 2.5F;
  const WidgetBarCapsuleSpec groupSpec = capsuleSpecFromGroup(bar, group);
  ok = expect(groupSpec.borderWidth == 2.5F, "capsule group border width reaches the resolved spec") && ok;

  // paw2-shell style presets: accent is the widget color, explicit keys still win.
  BarConfig plainBar;
  WidgetConfig styled;
  styled.settings["color"] = std::string("tertiary");
  const ColorSpec tertiary = colorSpecFromRole(ColorRole::Tertiary);

  styled.settings["style"] = std::string("box");
  const auto box = resolveWidgetBarCapsuleSpec(plainBar, &styled);
  ok = expect(box.enabled && box.border == std::optional<ColorSpec>(tertiary), "box: accent outline") && ok;
  ok = expect(box.fill == clearColorSpec() && box.radius == std::optional<float>(4.0F), "box: clear, square-ish") && ok;
  ok = expect(!box.underline.has_value(), "box: no underline") && ok;

  styled.settings["style"] = std::string("pill");
  const auto pill = resolveWidgetBarCapsuleSpec(plainBar, &styled);
  ok = expect(
           pill.enabled && pill.fill == plainBar.widgetCapsuleFill && !pill.underline.has_value(),
           "pill: the old name for the bar's capsule"
       )
      && ok;

  styled.settings["style"] = std::string("underline");
  const auto underline = resolveWidgetBarCapsuleSpec(plainBar, &styled);
  ok = expect(underline.enabled && underline.underline == std::optional<ColorSpec>(tertiary), "underline: accent") && ok;
  ok = expect(underline.fill == clearColorSpec() && !underline.border.has_value(), "underline: line only") && ok;

  styled.settings["style"] = std::string("none");
  plainBar.widgetCapsuleDefault = true;
  ok = expect(!resolveWidgetBarCapsuleSpec(plainBar, &styled).enabled, "none: overrides a capsule bar") && ok;
  plainBar.widgetCapsuleDefault = false;

  styled.settings["style"] = std::string("box");
  styled.settings["capsule_radius"] = 9.0;
  styled.settings["capsule_border"] = std::string("error");
  const auto overridden = resolveWidgetBarCapsuleSpec(plainBar, &styled);
  ok = expect(overridden.radius == std::optional<float>(9.0F), "explicit radius beats the preset") && ok;
  ok = expect(
           overridden.border == std::optional<ColorSpec>(colorSpecFromRole(ColorRole::Error)),
           "explicit border beats the preset"
       )
      && ok;

  WidgetConfig underlineOnly;
  underlineOnly.settings["underline"] = std::string("primary");
  underlineOnly.settings["underline_width"] = 3.0;
  const auto lineOnly = resolveWidgetBarCapsuleSpec(plainBar, &underlineOnly);
  ok = expect(lineOnly.enabled && lineOnly.fill == clearColorSpec(), "bare underline gets invisible geometry") && ok;
  ok = expect(lineOnly.underlineWidth == 3.0F, "underline width") && ok;

  // Workspaces' own `style = "underline"` underlines the focused workspace, never the whole widget.
  WidgetConfig workspacesStyled;
  workspacesStyled.type = "workspaces";
  workspacesStyled.settings["style"] = std::string("underline");
  const auto workspacesSpec = resolveWidgetBarCapsuleSpec(plainBar, &workspacesStyled);
  ok = expect(
           !workspacesSpec.enabled && !workspacesSpec.underline.has_value(),
           "workspaces style is not a whole-widget preset"
       )
      && ok;

  WidgetConfig inherit;
  plainBar.widgetCapsuleDefault = true;
  const auto inherited = resolveWidgetBarCapsuleSpec(plainBar, &inherit);
  ok = expect(inherited.enabled && !inherited.underline.has_value(), "no style keeps bar defaults") && ok;

  return ok ? 0 : 1;
}
