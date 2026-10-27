#pragma once

#include "render/core/color.h"
#include "ui/signal.h"

#include <cstddef>
#include <optional>

class CompositorPlatform;

namespace paw2shell::theme {

  // paw2's focus ring, titlebars, window shadows and overview backdrop follow the shell's palette.
  // They go to paw2's [colors] table in a drop-in of their own (Paw2ConfigWriter::writeThemeColors),
  // so the user's [appearance] and [overview] stay theirs: when the palette resolves, and when paw2
  // connects, which may be after the shell has started.
  class Paw2ThemeSync {
  public:
    explicit Paw2ThemeSync(CompositorPlatform& platform);
    ~Paw2ThemeSync();

    Paw2ThemeSync(const Paw2ThemeSync&) = delete;
    Paw2ThemeSync& operator=(const Paw2ThemeSync&) = delete;

    void setColors(const Color& focusRing, const Color& titlebar, const Color& shadow, const Color& backdrop);

  private:
    void refresh();

    CompositorPlatform& m_platform;
    Signal<>::ScopedConnection m_runtimeListener;
    bool m_paw2Connected = false;
    std::optional<Color> m_focusRing;
    std::optional<Color> m_titlebar;
    std::optional<Color> m_shadow;
    std::optional<Color> m_backdrop;
  };

} // namespace paw2shell::theme
