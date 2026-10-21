#pragma once

#include "shell/bar/widget_definition.h"

#include <string>

// The bar's door to the saved themes: a glyph button, drawn by WallpaperWidget,
// whose click opens the themes panel.
struct ThemesWidgetOptions {
  std::string glyph = "palette";
  std::string customImage;
  bool customImageColorize = false;
};

[[nodiscard]] const paw2shell::bar::WidgetDefinition<ThemesWidgetOptions>& themesWidgetDefinition();
