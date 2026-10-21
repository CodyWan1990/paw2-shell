#pragma once

#include "shell/bar/widget_definition.h"
#include "shell/bar/widgets/control_center_widget.h"

[[nodiscard]] const paw2shell::bar::WidgetDefinition<ControlCenterWidget::Options>& controlCenterWidgetDefinition();
