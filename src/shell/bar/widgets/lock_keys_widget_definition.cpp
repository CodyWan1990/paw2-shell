#include "shell/bar/widgets/lock_keys_widget_definition.h"

const paw2shell::bar::WidgetDefinition<LockKeysWidget::Options>& lockKeysWidgetDefinition() {
  using paw2shell::bar::field;
  using Options = LockKeysWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "lock_keys",
      .fields = {
          field<&Options::showCapsLock>({
              .key = "show_caps_lock",
          }),
          field<&Options::showNumLock>({
              .key = "show_num_lock",
          }),
          field<&Options::showScrollLock>({
              .key = "show_scroll_lock",
          }),
          field<&Options::hideWhenOff>({
              .key = "hide_when_off",
          }),
          field<&Options::displayMode>({
              .key = "display",
              .choices =
                  {
                      {
                          .value = LockKeysWidget::DisplayMode::Short,
                          .configValue = "short",
                          .labelKey = "settings.widgets.options.short",
                      },
                      {
                          .value = LockKeysWidget::DisplayMode::Full,
                          .configValue = "full",
                          .labelKey = "settings.widgets.options.full",
                      },
                  },
              .presentation = settings::WidgetSettingPresentation{
                  .segmented = true,
              },
          }),
      },
  };
  return definition;
}
