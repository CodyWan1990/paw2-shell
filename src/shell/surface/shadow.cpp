#include "shell/surface/shadow.h"

#include "render/core/color.h"

#include <algorithm>

namespace shell::surface_shadow {

  bool enabled(bool componentShadow, const ShellConfig::ShadowConfig& shadow) noexcept {
    return componentShadow && shadow.size > 0;
  }

  Bleed bleed(bool componentShadow, const ShellConfig::ShadowConfig& shadow) noexcept {
    if (!enabled(componentShadow, shadow)) {
      return {};
    }
    const auto offset = shadowDirectionOffset(shadow.direction);
    return {
        .left = shadow.size + std::max(0, -offset.x),
        .right = shadow.size + std::max(0, offset.x),
        .up = shadow.size + std::max(0, -offset.y),
        .down = shadow.size + std::max(0, offset.y),
    };
  }

  RoundedRectStyle
  style(const ShellConfig::ShadowConfig& shadow, float backgroundOpacity, const Shape& shape) noexcept {
    const auto offset = shadowDirectionOffset(shadow.direction);
    const float shadowAlpha = std::clamp(shadow.alpha, 0.0F, 1.0F) * std::clamp(backgroundOpacity, 0.0F, 1.0F);
    return RoundedRectStyle{
        .fill = rgba(0.0F, 0.0F, 0.0F, shadowAlpha),
        .border = Color{},
        .fillMode = FillMode::Solid,
        .corners = shape.corners,
        .logicalInset = shape.logicalInset,
        .radius = shape.radius,
        .softness = static_cast<float>(shadow.size),
        .borderWidth = 0.0F,
        .outerShadow = true,
        .shadowCutoutOffsetX = static_cast<float>(offset.x),
        .shadowCutoutOffsetY = static_cast<float>(offset.y),
    };
  }

  bool sameSurfaceMetrics(const ShellConfig::ShadowConfig& previous, const ShellConfig::ShadowConfig& next) noexcept {
    return previous.direction == next.direction && previous.size == next.size;
  }

  ShellConfig::ShadowConfig withOverrides(
      ShellConfig::ShadowConfig shadow, std::optional<std::int32_t> size, std::optional<float> alpha
  ) noexcept {
    if (size.has_value()) {
      shadow.size = *size;
    }
    if (alpha.has_value()) {
      shadow.alpha = *alpha;
    }
    return shadow;
  }

} // namespace shell::surface_shadow
