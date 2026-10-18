#pragma once

#include "render/core/color.h"

#include <cstddef>
#include <optional>

class CompositorPlatform;
class ConfigService;

namespace noctalia::theme {

  // With [shell] paw2_theme_colors on, paw2's focus ring, window shadows and overview backdrop
  // follow the shell's palette. They are paw2's settings, so they go through Paw2ConfigWriter like any Settings edit:
  // when the palette resolves, when the setting is turned on, and when paw2 connects, which may be
  // after the shell has started.
  class Paw2ThemeSync {
  public:
    Paw2ThemeSync(CompositorPlatform& platform, ConfigService& config);
    ~Paw2ThemeSync();

    Paw2ThemeSync(const Paw2ThemeSync&) = delete;
    Paw2ThemeSync& operator=(const Paw2ThemeSync&) = delete;

    void setColors(const Color& focusRing, const Color& shadow, const Color& backdrop);
    // Call on config reload: the setting may just have been turned on.
    void refresh();

  private:
    CompositorPlatform& m_platform;
    ConfigService& m_config;
    std::size_t m_runtimeListener = 0;
    bool m_paw2Connected = false;
    std::optional<Color> m_focusRing;
    std::optional<Color> m_shadow;
    std::optional<Color> m_backdrop;
  };

} // namespace noctalia::theme
