#include "shell/bar/widgets/paw2_layout_widget_definition.h"

const paw2shell::bar::WidgetDefinition<Paw2LayoutWidget::Options>& paw2LayoutWidgetDefinition() {
  using paw2shell::bar::field;
  using Options = Paw2LayoutWidget::Options;

  static const paw2shell::bar::WidgetDefinition<Options> definition{
      .type = "paw2_layout",
      .fields =
          {
              field<&Options::showGlyph>({
                  .key = "show_glyph",
              }),
              field<&Options::showLabel>({
                  .key = "show_label",
              }),
          },
      .glyph = [](const Options& /*options*/) { return std::string("layout-columns"); },
      .validateOptions = [](const Options& options) -> std::optional<std::string> {
        if (!options.showGlyph && !options.showLabel) {
          return "show_glyph and show_label cannot both be false";
        }
        return std::nullopt;
      },
  };
  return definition;
}
