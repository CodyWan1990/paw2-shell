#include "shell/bar/widgets/themes_widget_definition.h"

#include "shell/bar/widgets/glyph_button_definition.h"

const noctalia::bar::WidgetDefinition<ThemesWidgetOptions>& themesWidgetDefinition() {
  using Options = ThemesWidgetOptions;

  static const noctalia::bar::WidgetDefinition<Options> definition{
      .type = "themes",
      .fields = noctalia::bar::glyphButtonFields<Options>(),
      .glyph = [](const Options& options) { return options.glyph; },
  };
  return definition;
}
