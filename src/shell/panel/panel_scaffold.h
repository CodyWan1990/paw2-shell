#pragma once

#include "ui/builders.h"
#include "ui/style.h"

#include <memory>
#include <string>
#include <utility>

// Building blocks shared by the built-in panels, so each one lays out the same way:
// a padded root column, a glyph + title header with trailing controls, and a hint line.
namespace panel_scaffold {

  [[nodiscard]] inline std::unique_ptr<Flex> rootColumn(Flex** out, float scale, float gap = Style::spaceMd) {
    return ui::column({
        .out = out,
        .align = FlexAlign::Stretch,
        .gap = gap * scale,
        .padding = Style::spaceLg * scale,
    });
  }

  [[nodiscard]] inline std::unique_ptr<Label> title(std::string text, float scale, Label** out = nullptr) {
    return ui::label({
        .out = out,
        .text = std::move(text),
        .fontSize = Style::fontSizeTitle * scale,
        .fontWeight = FontWeight::Bold,
        .color = colorSpecFromRole(ColorRole::OnSurface),
    });
  }

  // Primary-coloured glyph and title, then any trailing controls (buttons, switches) pushed to
  // the far edge. Without trailing controls the glyph and title sit centred.
  template <typename... Trailing>
  [[nodiscard]] std::unique_ptr<Flex> header(std::string glyph, std::string text, float scale, Trailing&&... trailing) {
    constexpr bool kCentred = sizeof...(Trailing) == 0;
    auto heading = title(std::move(text), scale);
    if constexpr (!kCentred) {
      heading->setFlexGrow(1.0F);
    }
    return ui::row(
        {
            .align = FlexAlign::Center,
            .justify = kCentred ? FlexJustify::Center : FlexJustify::Start,
            .gap = Style::spaceSm * scale,
            .fillWidth = true,
        },
        ui::glyph({
            .glyph = std::move(glyph),
            .glyphSize = Style::fontSizeTitle * scale,
            .color = colorSpecFromRole(ColorRole::Primary),
        }),
        std::move(heading), std::forward<Trailing>(trailing)...
    );
  }

  // Centred caption listing the panel's keyboard shortcuts.
  [[nodiscard]] inline std::unique_ptr<Label> hint(std::string text, float scale) {
    return ui::label({
        .text = std::move(text),
        .fontSize = Style::fontSizeCaption * scale,
        .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
        .textAlign = TextAlign::Center,
    });
  }

} // namespace panel_scaffold
