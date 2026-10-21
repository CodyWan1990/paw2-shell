#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <string_view>

// One order of a sortable list: its value in saved state, and the glyph and tooltip of the
// button that cycles through the orders. A table lists the orders in cycling order, default first.
template <typename Mode> struct SortModeInfo {
  Mode mode;
  std::string_view state;
  std::string_view glyph;
  const char* tooltipKey;
};

namespace sort_modes {

  template <typename Table, typename Mode> [[nodiscard]] const auto& info(const Table& table, Mode mode) {
    const auto it = std::ranges::find_if(table, [mode](const auto& entry) { return entry.mode == mode; });
    return it != std::ranges::end(table) ? *it : *std::ranges::begin(table);
  }

  template <typename Table> [[nodiscard]] auto fromState(const Table& table, std::string_view state) {
    const auto it = std::ranges::find_if(table, [state](const auto& entry) { return entry.state == state; });
    return (it != std::ranges::end(table) ? *it : *std::ranges::begin(table)).mode;
  }

  // The order after `mode`, skipping any that `available` rejects; wraps to the default.
  template <typename Table, typename Mode>
  [[nodiscard]] Mode next(const Table& table, Mode mode, const std::function<bool(Mode)>& available = {}) {
    auto it = std::ranges::find_if(table, [mode](const auto& entry) { return entry.mode == mode; });
    const auto end = std::ranges::end(table);
    while (it != end && ++it != end) {
      if (!available || available(it->mode)) {
        return it->mode;
      }
    }
    return std::ranges::begin(table)->mode;
  }

} // namespace sort_modes
