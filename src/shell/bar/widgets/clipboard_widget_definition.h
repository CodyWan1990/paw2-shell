#pragma once

#include "shell/bar/widget_definition.h"
#include "shell/bar/widgets/clipboard_widget.h"

[[nodiscard]] const paw2shell::bar::WidgetDefinition<ClipboardWidget::Options>& clipboardWidgetDefinition();
