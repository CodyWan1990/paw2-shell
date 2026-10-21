#include "shell/bar/widgets/test_widget_definition.h"

const paw2shell::bar::WidgetDefinition<std::monostate>& testWidgetDefinition() {
  static const paw2shell::bar::WidgetDefinition<std::monostate> definition{
      .type = "test",
  };
  return definition;
}
