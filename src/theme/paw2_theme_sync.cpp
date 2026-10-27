#include "theme/paw2_theme_sync.h"

#include "compositors/compositor_platform.h"
#include "compositors/paw2/paw2_config_writer.h"
#include "compositors/paw2/paw2_runtime.h"

namespace paw2shell::theme {

  Paw2ThemeSync::Paw2ThemeSync(CompositorPlatform& platform) : m_platform(platform) {
    m_runtimeListener = m_platform.paw2Runtime().addListener([this]() {
      const bool connected = m_platform.paw2Runtime().trusted();
      if (connected && !m_paw2Connected) {
        refresh();
      }
      m_paw2Connected = connected;
    });
  }

  Paw2ThemeSync::~Paw2ThemeSync() = default;

  void
  Paw2ThemeSync::setColors(const Color& focusRing, const Color& titlebar, const Color& shadow, const Color& backdrop) {
    m_focusRing = focusRing;
    m_titlebar = titlebar;
    m_shadow = shadow;
    m_backdrop = backdrop;
    refresh();
  }

  void Paw2ThemeSync::refresh() {
    if (!m_focusRing.has_value() || !m_titlebar.has_value() || !m_shadow.has_value() || !m_backdrop.has_value()) {
      return;
    }
    // Some palettes' shadow role is their surface colour, which is also the wallpaper-less background, so a
    // shadow in it vanishes. Pull it most of the way to black: it keeps a hint of the palette and always
    // reads darker than what it falls on.
    m_platform.paw2ConfigWriter().writeThemeColors({
        {"focus_ring", formatRgbHex(*m_focusRing)},
        {"titlebar", formatRgbHex(*m_titlebar)},
        {"shadow", formatRgbHex(lerpColor(*m_shadow, rgba(0.0F, 0.0F, 0.0F), 0.75F))},
        {"backdrop", formatRgbHex(*m_backdrop)},
    });
  }

} // namespace paw2shell::theme
