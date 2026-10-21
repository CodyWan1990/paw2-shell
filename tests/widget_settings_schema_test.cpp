// The validator and the settings GUI must agree on which keys a bar widget accepts.
// Generic bar-widget settings (scale, color, anchor, capsule_*, ...) are written by the GUI,
// so the validator schema has to list them too, or `config validate` warns "unknown setting"
// about its own output.
#include "shell/settings/widget_settings_registry.h"

#include <algorithm>
#include <cstdlib>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {

  bool expect(bool condition, const char* message) {
    if (!condition) {
      std::println(stderr, "widget_settings_schema_test: {}", message);
    }
    return condition;
  }

  bool hasKey(const noctalia::config::schema::WidgetSettingSchema& schema, std::string_view key) {
    return std::ranges::any_of(schema, [&](const noctalia::config::schema::WidgetSettingField& field) {
      return field.key == key;
    });
  }

} // namespace

int main() {
  bool ok = true;
  const auto schema = settings::widgetSettingSchema("clock", nullptr);
  for (const std::string_view key :
       {"enabled", "anchor", "interactive", "scale", "font_scale", "color", "icon_color", "capsule", "capsule_fill",
        "capsule_padding", "actions"}) {
    ok = expect(hasKey(schema, key), "common widget setting missing from the clock widget schema") && ok;
    if (!hasKey(schema, key)) {
      std::println(stderr, "  missing key: {}", key);
    }
  }

  // paw2-shell's editor order: Look (style, color, underline width) first and basic, then the
  // widget's own options, then Noctalia's fine-tuning behind Advanced.
  {
    const auto specs = settings::widgetSettingSpecs("volume", nullptr, "sans-serif", false);
    std::vector<const settings::WidgetSettingSpec*> shown;
    for (const auto& spec : specs) {
      if (spec.visibleInInspector) {
        shown.push_back(&spec);
      }
    }
    ok = expect(shown.size() > 3, "volume has inspector settings") && ok;
    if (shown.size() > 3) {
      ok = expect(shown[0]->schema.key == "style" && shown[0]->segmented, "style leads, segmented") && ok;
      ok = expect(shown[1]->schema.key == "color", "color follows style") && ok;
      ok = expect(shown[2]->schema.key == "underline_width", "underline width completes the look") && ok;
      ok = expect(shown[3]->group != "look", "the widget's own options come next") && ok;
    }
    const auto find = [&specs](std::string_view key) {
      return std::ranges::find_if(specs, [key](const auto& spec) { return spec.schema.key == key; });
    };
    for (std::string_view key : {"style", "color", "underline_width", "show_glyph", "show_label"}) {
      const auto it = find(key);
      ok = expect(it != specs.end() && !it->advanced, "look and own options are basic") && ok;
    }
    for (std::string_view key : {"capsule", "capsule_fill", "scale", "font_family", "actions", "icon_color"}) {
      const auto it = find(key);
      ok = expect(it != specs.end() && it->advanced, "fine-tuning is advanced") && ok;
    }
    const auto style = find("style");
    const bool offersPill = style != specs.end()
        && std::ranges::any_of(style->options, [](const auto& option) { return option.value == "pill"; });
    ok = expect(!offersPill, "pill is no longer offered; capsule replaced it") && ok;
  }
  // Workspaces keeps its own `style` (regular/minimal/focus_hint), not the preset one.
  {
    const auto specs = settings::widgetSettingSpecs("workspaces", nullptr, "sans-serif", false);
    const auto styles = std::ranges::count_if(specs, [](const auto& spec) { return spec.schema.key == "style"; });
    ok = expect(styles == 1, "workspaces has exactly one style setting") && ok;
    const auto style = std::ranges::find_if(specs, [](const auto& spec) { return spec.schema.key == "style"; });
    std::vector<std::string> values;
    if (style != specs.end()) {
      for (const auto& option : style->options) {
        values.push_back(option.value);
      }
    }
    ok = expect(
             values == std::vector<std::string>{"regular", "minimal", "underline", "focus_hint"},
             "workspaces styles: pills (regular), minimal, underline, focus hint"
         )
        && ok;
    const auto overrides =
        std::ranges::find_if(specs, [](const auto& spec) { return spec.schema.key == "label_overrides"; });
    ok = expect(overrides != specs.end() && !overrides->advanced, "workspace names are a basic setting") && ok;
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
