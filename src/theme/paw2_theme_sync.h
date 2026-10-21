#pragma once

#include "render/core/color.h"
#include "ui/signal.h"

#include <cstddef>
#include <optional>

class CompositorPlatform;
class ConfigService;

namespace paw2shell::theme {

  // paw2's focus ring, titlebars, window shadows and overview backdrop follow the shell's palette.
  // They are paw2's settings, so they go through Paw2ConfigWriter like any Settings edit: when the
  // palette resolves, and when paw2 connects, which may be after the shell has started.
  class Paw2ThemeSync {
  public:
    Paw2ThemeSync(CompositorPlatform& platform, ConfigService& config);
    ~Paw2ThemeSync();

    Paw2ThemeSync(const Paw2ThemeSync&) = delete;
    Paw2ThemeSync& operator=(const Paw2ThemeSync&) = delete;

    void setColors(const Color& focusRing, const Color& titlebar, const Color& shadow, const Color& backdrop);
    // Call on config reload: the font may have changed.
    void refresh();

  private:
    CompositorPlatform& m_platform;
    ConfigService& m_config;
    Signal<>::ScopedConnection m_runtimeListener;
    bool m_paw2Connected = false;
    std::optional<Color> m_focusRing;
    std::optional<Color> m_titlebar;
    std::optional<Color> m_shadow;
    std::optional<Color> m_backdrop;
  };

} // namespace paw2shell::theme
