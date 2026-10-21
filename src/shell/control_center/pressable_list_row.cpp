#include "shell/control_center/pressable_list_row.h"

#include "render/scene/input_area.h"
#include "ui/builders.h"
#include "ui/controls/label.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <algorithm>
#include <utility>

namespace control_center {

  void applyListRowStyle(Flex& row, float scale, float minHeight) {
    row.setDirection(FlexDirection::Horizontal);
    row.setAlign(FlexAlign::Center);
    row.setGap(Style::spaceSm * scale);
    row.setPadding(Style::spaceSm * scale, Style::spaceMd * scale);
    row.setMinHeight(minHeight * scale);
    row.setRadius(Style::scaledRadiusMd(scale));
    row.setFill(colorSpecFromRole(ColorRole::Surface));
    row.clearBorder();
  }

  PressableListRow::PressableListRow(float scale, float minHeight) { applyListRowStyle(*this, scale, minHeight); }

  void PressableListRow::addHitArea(Label* title, std::function<void()> onClick, Node* hitAreaEnd) {
    m_title = title;
    m_hitAreaEnd = hitAreaEnd;
    auto area = ui::inputArea({});
    area->setPropagateEvents(true);
    area->setOnEnter([this](const InputArea::PointerData& /*data*/) { applyState(); });
    area->setOnLeave([this]() { applyState(); });
    area->setOnPress([this](const InputArea::PointerData& /*data*/) { applyState(); });
    area->setOnClick([onClick = std::move(onClick)](const InputArea::PointerData& /*data*/) {
      if (onClick) {
        onClick();
      }
    });
    m_inputArea = static_cast<InputArea*>(addChild(std::move(area)));
    applyState();
    m_paletteConn = paletteChanged().connect([this] { applyState(); });
  }

  void PressableListRow::doLayout(Renderer& renderer) {
    if (m_inputArea == nullptr) {
      Flex::doLayout(renderer);
      return;
    }
    // The area must not take part in the row's own layout.
    m_inputArea->setVisible(false);
    Flex::doLayout(renderer);
    m_inputArea->setVisible(true);
    m_inputArea->setPosition(0.0F, 0.0F);
    const float areaWidth = m_hitAreaEnd != nullptr ? std::max(0.0F, m_hitAreaEnd->x() - gap()) : width();
    m_inputArea->setSize(areaWidth, height());
    applyState();
  }

  LayoutSize PressableListRow::doMeasure(Renderer& renderer, const LayoutConstraints& constraints) {
    return measureByLayout(renderer, constraints);
  }

  void PressableListRow::doArrange(Renderer& renderer, const LayoutRect& rect) { arrangeByLayout(renderer, rect); }

  void PressableListRow::applyState() {
    const bool pressed = m_inputArea != nullptr && m_inputArea->pressed();
    const bool hovered = m_inputArea != nullptr && m_inputArea->hovered();
    setFill(colorSpecFromRole(pressed ? ColorRole::Primary : ColorRole::Surface));
    if (pressed || hovered) {
      setBorder(colorSpecFromRole(pressed ? ColorRole::Primary : ColorRole::Hover), Style::borderWidth);
    } else {
      clearBorder();
    }
    if (m_title != nullptr) {
      m_title->setColor(colorSpecFromRole(pressed ? ColorRole::OnPrimary : ColorRole::OnSurface));
    }
  }

} // namespace control_center
