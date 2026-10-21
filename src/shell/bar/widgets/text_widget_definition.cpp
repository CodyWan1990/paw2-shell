#include "shell/bar/widgets/text_widget_definition.h"

const paw2shell::bar::WidgetDefinition<TextWidget::Options>& textWidgetDefinition() {
  using paw2shell::bar::field;
  using Options = TextWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "text",
      .fields = {
          field<&Options::text>({
              .key = "text",
          }),
      },
  };
  return definition;
}
