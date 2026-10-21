#pragma once

#include "shell/bar/widget_definition.h"
#include "shell/bar/widgets/launcher_widget.h"

[[nodiscard]] const paw2shell::bar::WidgetDefinition<LauncherWidget::Options>& launcherWidgetDefinition();
