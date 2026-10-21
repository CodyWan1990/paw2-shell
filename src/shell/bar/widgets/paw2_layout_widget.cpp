#include "shell/bar/widgets/paw2_layout_widget.h"

#include "compositors/compositor_platform.h"
#include "compositors/paw2/paw2_runtime.h"
#include "render/core/renderer.h"
#include "render/scene/input_area.h"
#include "ui/builders.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {

  [[nodiscard]] std::string_view glyphForLayout(std::string_view layout) {
    if (layout == "scrolling") {
      return "layout-columns";
    }
    if (layout == "dwindle") {
      return "layout-dashboard";
    }
    if (layout == "master") {
      return "layout-sidebar";
    }
    if (layout == "monocle") {
      return "square";
    }
    return "layout";
  }

} // namespace

Paw2LayoutWidget::Paw2LayoutWidget(CompositorPlatform& platform, wl_output* output, Options options)
    : m_platform(platform), m_output(output), m_showGlyph(options.showGlyph), m_showLabel(options.showLabel) {
  m_listenerToken = m_platform.paw2Runtime().addListener([this]() { requestUpdate(); });
}

Paw2LayoutWidget::~Paw2LayoutWidget() = default;

void Paw2LayoutWidget::create() {
  auto area = ui::inputArea({});
  area->addChild(
      ui::glyph({
          .out = &m_glyph,
          .glyph = std::string(glyphForLayout({})),
          .glyphSize = Style::baseGlyphSize * m_contentScale,
          .color = widgetIconColorOr(colorSpecFromRole(ColorRole::OnSurface)),
      })
  );
  area->addChild(
      ui::label({
          .out = &m_label,
          .text = {},
          .fontSize = Style::fontSizeBody * fontScale(),
          .fontWeight = labelFontWeight(),
          .fontFamily = labelFontFamily(),
      })
  );
  setRoot(std::move(area));
}

std::string Paw2LayoutWidget::currentLayout() const {
  const auto& paw2 = m_platform.paw2Runtime();
  const auto* info = m_platform.findOutputByWl(m_output);
  const auto* output = info != nullptr ? paw2.findOutput(info->connectorName) : nullptr;
  if (output == nullptr) {
    output = paw2.focusedOutput();
  }
  return output != nullptr ? output->layout : std::string{};
}

void Paw2LayoutWidget::doUpdate(Renderer& renderer) { sync(renderer); }

void Paw2LayoutWidget::sync(Renderer& renderer) {
  if (m_glyph == nullptr || m_label == nullptr || root() == nullptr) {
    return;
  }
  const std::string layout = currentLayout();
  root()->setVisible(!layout.empty());
  if (layout == m_lastLayout) {
    return;
  }
  m_lastLayout = layout;
  m_glyph->setGlyph(std::string(glyphForLayout(layout)));
  m_glyph->measure(renderer);
  m_label->setText(layout);
  m_label->measure(renderer);
  if (auto* area = static_cast<InputArea*>(root()); area != nullptr) {
    area->setTooltip(layout);
  }
  requestRedraw();
}

void Paw2LayoutWidget::doLayout(Renderer& renderer, float containerWidth, float containerHeight) {
  if (m_glyph == nullptr || m_label == nullptr || root() == nullptr) {
    return;
  }
  m_isVertical = containerHeight > containerWidth;
  sync(renderer);
  if (!root()->visible()) {
    root()->setSize(0.0F, 0.0F);
    return;
  }

  // A vertical bar has no room for a word: the glyph says it.
  const bool showLabel = m_showLabel && !m_isVertical;
  const bool showGlyph = m_showGlyph || !showLabel;
  m_glyph->setVisible(showGlyph);
  m_label->setVisible(showLabel);

  m_glyph->setGlyphSize(Style::baseGlyphSize * m_contentScale);
  m_glyph->setColor(widgetIconColorOr(colorSpecFromRole(ColorRole::OnSurface)));
  m_glyph->measure(renderer);
  m_label->setColor(widgetForegroundOr(colorSpecFromRole(ColorRole::OnSurface)));
  m_label->measure(renderer);

  const float glyphW = showGlyph ? m_glyph->width() : 0.0F;
  const float glyphH = showGlyph ? m_glyph->height() : 0.0F;
  const float labelW = showLabel ? m_label->width() : 0.0F;
  const float labelH = showLabel ? m_label->height() : 0.0F;
  const float height = std::max(glyphH, labelH);
  float x = 0.0F;
  if (showGlyph) {
    m_glyph->setPosition(0.0F, std::round((height - glyphH) * 0.5F));
    x += glyphW + (showLabel ? Style::spaceXs : 0.0F);
  }
  if (showLabel) {
    m_label->setPosition(x, std::round((height - labelH) * 0.5F));
    x += labelW;
  }
  root()->setSize(x, height);
}
