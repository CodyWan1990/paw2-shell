#include "shell/bar/widgets/nightlight_widget_definition.h"

const paw2shell::bar::WidgetDefinition<std::monostate>& nightlightWidgetDefinition() {
  static const paw2shell::bar::WidgetDefinition<std::monostate> definition{
      .type = "nightlight",
  };
  return definition;
}
