#pragma once

#include "shell/bar/widget_definition.h"
#include "shell/bar/widgets/taskbar_widget.h"

[[nodiscard]] const paw2shell::bar::WidgetDefinition<TaskbarWidgetOptions>& taskbarWidgetDefinition();
