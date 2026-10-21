#include "shell/control_center/slide_transition.h"

#include "render/animation/animation.h"
#include "render/scene/node.h"
#include "shell/panel/panel_manager.h"
#include "ui/style.h"

#include <utility>

bool SlideTransition::slideOut(Node* target, const Node* travel, int direction, std::function<void()> onSwapped) {
  cancel(target);
  AnimationManager* animations = target != nullptr ? target->animationManager() : nullptr;
  if (animations == nullptr || travel == nullptr) {
    return false;
  }

  m_animations = animations;
  m_direction = direction > 0 ? 1 : -1;
  m_baseX = target->x();
  m_baseY = target->y();

  PanelManager::instance().requestFrameTick();
  m_animId = animations->animate(
      0.0F, 1.0F, static_cast<float>(Style::animFast), Easing::EaseOutCubic,
      [this, target, travel](float progress) {
        apply(*target, *travel, progress, false);
        PanelManager::instance().requestRedraw();
      },
      [this, onSwapped = std::move(onSwapped)]() {
        m_animId = 0;
        m_pendingSlideIn = true;
        onSwapped();
      },
      target
  );
  return true;
}

void SlideTransition::slideIn(Node* target, const Node* travel) {
  m_pendingSlideIn = false;
  AnimationManager* animations = target != nullptr ? target->animationManager() : nullptr;
  if (animations == nullptr || travel == nullptr) {
    return;
  }

  m_animations = animations;
  m_baseX = target->x();
  m_baseY = target->y();
  apply(*target, *travel, 0.0F, true);
  PanelManager::instance().requestFrameTick();
  m_animId = animations->animate(
      0.0F, 1.0F, static_cast<float>(Style::animFast), Easing::EaseOutCubic,
      [this, target, travel](float progress) {
        apply(*target, *travel, progress, true);
        PanelManager::instance().requestRedraw();
      },
      [this, target]() {
        m_animId = 0;
        target->setPosition(m_baseX, m_baseY);
        target->setOpacity(1.0F);
      },
      target
  );
}

void SlideTransition::cancel(Node* target) {
  const bool wasAnimating = m_animId != 0;
  if (wasAnimating && m_animations != nullptr) {
    m_animations->cancel(m_animId);
  }
  m_animId = 0;
  m_pendingSlideIn = false;
  if (wasAnimating && target != nullptr) {
    target->setPosition(m_baseX, m_baseY);
    target->setOpacity(1.0F);
  }
}

void SlideTransition::apply(Node& target, const Node& travel, float progress, bool slidingIn) const {
  const float distance = travel.width();
  if (distance <= 0.0F) {
    return;
  }
  const auto direction = static_cast<float>(m_direction);
  if (slidingIn) {
    target.setPosition(m_baseX + direction * distance * (1.0F - progress), m_baseY);
    target.setOpacity(0.7F + 0.3F * progress);
  } else {
    target.setPosition(m_baseX - direction * distance * progress, m_baseY);
    target.setOpacity(1.0F - 0.3F * progress);
  }
}
