#include "shell/bar/widgets/brightness_widget_definition.h"

const paw2shell::bar::WidgetDefinition<BrightnessWidget::Options>& brightnessWidgetDefinition() {
  using paw2shell::bar::field;
  using Options = BrightnessWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "brightness",
      .fields = {
          field<&Options::showLabel>({
              .key = "show_label",
          }),
          field<&Options::showGlyph>({
              .key = "show_glyph",
          }),
          field<&Options::showWhenUnavailable>({
              .key = "show_when_unavailable",
          }),
      },
  };
  return definition;
}
