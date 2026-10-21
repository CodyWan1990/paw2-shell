#include "shell/bar/widgets/screenshot_widget_definition.h"

#include "shell/bar/widgets/glyph_button_definition.h"

const paw2shell::bar::WidgetDefinition<ScreenshotWidget::Options>& screenshotWidgetDefinition() {
  using Options = ScreenshotWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "screenshot",
      .fields = paw2shell::bar::glyphButtonFields<Options>(),
  };
  return definition;
}
