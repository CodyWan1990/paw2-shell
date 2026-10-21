#pragma once

#include "ui/controls/flex.h"
#include "ui/signal.h"

#include <functional>

class InputArea;
class Label;
class Renderer;

namespace control_center {

  // Look of a list row in a control-center card: horizontal, centred, surface fill, rounded.
  void applyListRowStyle(Flex& row, float scale, float minHeight);

  // A list row (audio device, access point, VPN) that lights up on hover and press and runs
  // an action when clicked. Subclasses add their children, then call addHitArea().
  class PressableListRow : public Flex {
  public:
    PressableListRow(float scale, float minHeight);

    void doLayout(Renderer& renderer) override;
    LayoutSize doMeasure(Renderer& renderer, const LayoutConstraints& constraints) override;
    void doArrange(Renderer& renderer, const LayoutRect& rect) override;

  protected:
    // Covers the row with the click area. `title` follows the pressed colour; the area stops
    // short of `hitAreaEnd` (a trailing button with its own action) when one is given.
    void addHitArea(Label* title, std::function<void()> onClick, Node* hitAreaEnd = nullptr);
    void applyState();

  private:
    InputArea* m_inputArea = nullptr;
    Label* m_title = nullptr;
    Node* m_hitAreaEnd = nullptr;
    Signal<>::ScopedConnection m_paletteConn;
  };

} // namespace control_center
