#pragma once

#include <cstdint>
#include <string_view>

namespace compositors {

  // paw2-shell targets paw2 only. Unknown keeps the shell usable (generic Wayland protocols
  // only) when started outside a paw2 session, e.g. nested under another compositor for testing.
  enum class CompositorKind : std::uint8_t {
    Unknown = 0,
    Paw2 = 1,
  };

  // Detected once per process from env vars. Cached after the first call.
  [[nodiscard]] CompositorKind detect();

  // Human-readable name (e.g. for diagnostics / hardware info). "Unknown" for unrecognized.
  [[nodiscard]] std::string_view name(CompositorKind kind);

  // Concatenated XDG_CURRENT_DESKTOP:XDG_SESSION_DESKTOP:DESKTOP_SESSION env hint for diagnostics.
  [[nodiscard]] std::string_view envHint();

  [[nodiscard]] inline bool isPaw2() { return detect() == CompositorKind::Paw2; }

} // namespace compositors
