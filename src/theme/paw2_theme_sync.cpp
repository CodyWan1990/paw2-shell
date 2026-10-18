#include "theme/paw2_theme_sync.h"

#include "compositors/compositor_platform.h"
#include "compositors/paw2/paw2_config_writer.h"
#include "compositors/paw2/paw2_runtime.h"
#include "config/config_service.h"

namespace noctalia::theme {

  Paw2ThemeSync::Paw2ThemeSync(CompositorPlatform& platform, ConfigService& config)
      : m_platform(platform), m_config(config) {
    m_runtimeListener = m_platform.paw2Runtime().addListener([this]() {
      const bool connected = m_platform.paw2Runtime().trusted();
      if (connected && !m_paw2Connected) {
        refresh();
      }
      m_paw2Connected = connected;
    });
  }

  Paw2ThemeSync::~Paw2ThemeSync() { m_platform.paw2Runtime().removeListener(m_runtimeListener); }

  void Paw2ThemeSync::setColors(const Color& focusRing, const Color& backdrop) {
    m_focusRing = focusRing;
    m_backdrop = backdrop;
    refresh();
  }

  void Paw2ThemeSync::refresh() {
    if (!m_config.config().shell.paw2ThemeColors || !m_focusRing.has_value() || !m_backdrop.has_value()) {
      return;
    }
    // Unchanged values are no-ops in the writer, so a reload that touched nothing writes nothing.
    auto& writer = m_platform.paw2ConfigWriter();
    writer.set("appearance.focus_ring_color", formatRgbHex(*m_focusRing));
    writer.set("overview.backdrop_color", formatRgbHex(*m_backdrop));
  }

} // namespace noctalia::theme
