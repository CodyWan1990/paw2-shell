#pragma once

#include "render/animation/animation_manager.h"

#include <functional>

class Node;

// Sideways swap used by tabs that page through content in place (calendar months, forecast
// views, notification filters, screen-time ranges). The target slides out and fades, the
// caller swaps its content in onSwapped, and once the new content is laid out slideIn()
// brings it back from the other side. The animation is owned by the target node.
class SlideTransition {
public:
  // No slide running or waiting for slideIn().
  [[nodiscard]] bool idle() const noexcept { return m_animId == 0 && !m_pendingSlideIn; }
  // Sliding out: the old content is still showing, so the caller should not rebuild it.
  [[nodiscard]] bool slidingOut() const noexcept { return m_animId != 0 && !m_pendingSlideIn; }
  // Slid out and swapped; the caller should rebuild, lay out, then call slideIn().
  [[nodiscard]] bool pendingSlideIn() const noexcept { return m_pendingSlideIn; }

  // Slides `target` across the width of `travel` (direction > 0 moves it left). Returns false
  // without animating when the target has no animation manager; the caller then swaps directly.
  bool slideOut(Node* target, const Node* travel, int direction, std::function<void()> onSwapped);
  void slideIn(Node* target, const Node* travel);
  // Stops a running slide and puts the target back where it rests.
  void cancel(Node* target);

private:
  void apply(Node& target, const Node& travel, float progress, bool slidingIn) const;

  AnimationManager* m_animations = nullptr;
  AnimationManager::Id m_animId = 0;
  int m_direction = 0;
  float m_baseX = 0.0F;
  float m_baseY = 0.0F;
  bool m_pendingSlideIn = false;
};
