#pragma once

#include "core/input/key_symbols.h"
#include "core/input/keybind_matcher.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ui {

  // Index step for a grid navigation key on a row-major grid: ±1 for Left/Right, ±columns for
  // Up/Down and ±pageStride for PageUp/PageDown (when pageStride > 0). nullopt for any other key.
  [[nodiscard]] inline std::optional<std::ptrdiff_t>
  gridNavDelta(std::uint32_t sym, std::uint32_t modifiers, std::size_t columns, std::size_t pageStride = 0) {
    const auto cols = static_cast<std::ptrdiff_t>(std::max<std::size_t>(columns, 1));
    if (pageStride > 0 && KeySymbol::isPageUp(sym)) {
      return -static_cast<std::ptrdiff_t>(pageStride);
    }
    if (pageStride > 0 && KeySymbol::isPageDown(sym)) {
      return static_cast<std::ptrdiff_t>(pageStride);
    }
    if (KeybindMatcher::matches(KeybindAction::Left, sym, modifiers)) {
      return -1;
    }
    if (KeybindMatcher::matches(KeybindAction::Right, sym, modifiers)) {
      return 1;
    }
    if (KeybindMatcher::matches(KeybindAction::Up, sym, modifiers)) {
      return -cols;
    }
    if (KeybindMatcher::matches(KeybindAction::Down, sym, modifiers)) {
      return cols;
    }
    return std::nullopt;
  }

  // Where a step of `delta` lands among `count` items. With no current item, forward steps select
  // the first and backward steps the last. A step past either end clamps to it when clampToEnds,
  // otherwise it leaves the selection where it was.
  [[nodiscard]] inline std::size_t
  gridNavTarget(std::optional<std::size_t> current, std::ptrdiff_t delta, std::size_t count, bool clampToEnds = true) {
    if (count == 0) {
      return 0;
    }
    const auto last = static_cast<std::ptrdiff_t>(count - 1);
    if (!current.has_value() || static_cast<std::ptrdiff_t>(*current) > last) {
      return delta < 0 ? count - 1 : 0;
    }
    const auto next = static_cast<std::ptrdiff_t>(*current) + delta;
    if (next >= 0 && next <= last) {
      return static_cast<std::size_t>(next);
    }
    return clampToEnds ? static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(next, 0, last)) : *current;
  }

} // namespace ui
