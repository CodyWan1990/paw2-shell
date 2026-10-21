#pragma once

#include "wayland/layer_surface.h"
#include "wayland/wayland_connection.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <cstdint>

// Layer-shell popups inherit their parent's keyboard interactivity. A bar is None, so a
// grabbing popup over it would get no keyboard focus and ESC could not reach it. grab()
// flips the parent to OnDemand before the popup maps; release() puts it back to None.
class ParentKeyboardGrab {
public:
  void grab(zwlr_layer_surface_v1* layerSurface, wl_surface* wlSurface) {
    if (layerSurface == nullptr || wlSurface == nullptr) {
      return;
    }
    m_layerSurface = layerSurface;
    m_wlSurface = wlSurface;
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layerSurface, static_cast<std::uint32_t>(LayerShellKeyboard::OnDemand)
    );
    wl_surface_commit(wlSurface);
  }

  void release(const WaylandConnection* wayland) {
    zwlr_layer_surface_v1* layerSurface = m_layerSurface;
    wl_surface* wlSurface = m_wlSurface;
    m_layerSurface = nullptr;
    m_wlSurface = nullptr;
    // The parent can be destroyed while the popup is open (bar rebuild, output removal).
    // Its proxies are then freed, so only touch it while still registered.
    if (layerSurface == nullptr || wayland == nullptr || wayland->layerSurfaceFor(wlSurface) != layerSurface) {
      return;
    }
    zwlr_layer_surface_v1_set_keyboard_interactivity(layerSurface, static_cast<std::uint32_t>(LayerShellKeyboard::None));
    wl_surface_commit(wlSurface);
  }

private:
  zwlr_layer_surface_v1* m_layerSurface = nullptr;
  wl_surface* m_wlSurface = nullptr;
};
