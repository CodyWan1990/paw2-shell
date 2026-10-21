#include "shell/bar/widgets/caffeine_widget_definition.h"

const paw2shell::bar::WidgetDefinition<std::monostate>& caffeineWidgetDefinition() {
  static const paw2shell::bar::WidgetDefinition<std::monostate> definition{
      .type = "caffeine",
  };
  return definition;
}
