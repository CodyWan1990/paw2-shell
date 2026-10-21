#pragma once

#include "shell/bar/widget.h"
#include "ui/signal.h"

#include <cstddef>
#include <string>

class CompositorPlatform;
class Glyph;
class Label;
class Renderer;
struct wl_output;

// The tiling layout paw2 is using on this bar's monitor (scrolling, dwindle, master, monocle).
// Clicking or scrolling steps through paw2's enabled layouts via `paw2-layout`. Hidden when the
// shell is not running on paw2.
class Paw2LayoutWidget : public Widget {
public:
  struct Options {
    bool showGlyph = true;
    bool showLabel = true;
  };

  Paw2LayoutWidget(CompositorPlatform& platform, wl_output* output, Options options);
  ~Paw2LayoutWidget() override;

  Paw2LayoutWidget(const Paw2LayoutWidget&) = delete;
  Paw2LayoutWidget& operator=(const Paw2LayoutWidget&) = delete;

  void create() override;

private:
  void doLayout(Renderer& renderer, float containerWidth, float containerHeight) override;
  void doUpdate(Renderer& renderer) override;
  void sync(Renderer& renderer);
  [[nodiscard]] std::string currentLayout() const;

  CompositorPlatform& m_platform;
  wl_output* m_output = nullptr;
  bool m_showGlyph = true;
  bool m_showLabel = true;
  Signal<>::ScopedConnection m_listenerToken;

  Glyph* m_glyph = nullptr;
  Label* m_label = nullptr;
  std::string m_lastLayout;
  bool m_isVertical = false;
};
