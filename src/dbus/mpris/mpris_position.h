#pragma once

#include <chrono>
#include <cstdint>

namespace mpris {

  // A position jump back by more than this is not playback progress.
  inline constexpr std::int64_t kPositionRegressionDeltaUs = 5'000'000;

  // Playing players sometimes report a position near zero for a moment (metadata refresh,
  // buffering). True when `liveUs` looks like such a blip under `displayedUs`, sampled at
  // `sampledAt`, so the caller keeps showing `displayedUs` for the same playing track.
  [[nodiscard]] inline bool isTransientPositionRegression(
      std::int64_t displayedUs, std::int64_t liveUs, std::chrono::steady_clock::time_point sampledAt,
      std::chrono::steady_clock::time_point now
  ) {
    constexpr auto kWindow = std::chrono::milliseconds(1500);
    constexpr std::int64_t kFloorUs = 5'000'000;
    constexpr std::int64_t kCeilingUs = 1'500'000;
    return sampledAt != std::chrono::steady_clock::time_point{}
    && now - sampledAt <= kWindow
        && displayedUs >= kFloorUs
        && liveUs <= kCeilingUs
        && liveUs + kPositionRegressionDeltaUs < displayedUs;
  }

} // namespace mpris
