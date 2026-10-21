#include "shell/bar/widgets/notification_widget_definition.h"

const paw2shell::bar::WidgetDefinition<NotificationWidget::Options>& notificationWidgetDefinition() {
  using paw2shell::bar::field;
  using Options = NotificationWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "notifications",
      .fields = {
          field<&Options::hideWhenNoUnread>({
              .key = "hide_when_no_unread",
          }),
      },
  };
  return definition;
}
