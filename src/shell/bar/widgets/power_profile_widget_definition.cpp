#include "shell/bar/widgets/power_profile_widget_definition.h"

const paw2shell::bar::WidgetDefinition<std::monostate>& powerProfileWidgetDefinition() {
  static const paw2shell::bar::WidgetDefinition<std::monostate> definition{
      .type = "power_profile",
  };
  return definition;
}
