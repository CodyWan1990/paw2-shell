#include "shell/bar/widgets/themes_widget_definition.h"

#include "shell/bar/widgets/glyph_button_definition.h"

const paw2shell::bar::WidgetDefinition<ThemesWidgetOptions>& themesWidgetDefinition() {
  using Options = ThemesWidgetOptions;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "themes",
      .fields = paw2shell::bar::glyphButtonFields<Options>(),
      .glyph = [](const Options& options) { return options.glyph; },
  };
  return definition;
}
