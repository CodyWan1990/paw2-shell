#pragma once

#include <cstdint>
#include <optional>
#include <xkbcommon/xkbcommon.h>

namespace input {

  // Latin letter for this key across layouts (e.g. Russian ф → a). nullopt if none.
  [[nodiscard]] inline std::optional<std::uint32_t>
  latinShortcutKeysym(xkb_keymap* keymap, xkb_keycode_t keycode) noexcept {
    if (keymap == nullptr) {
      return std::nullopt;
    }

    const xkb_layout_index_t layoutCount = xkb_keymap_num_layouts_for_key(keymap, keycode);
    for (xkb_layout_index_t layout = 0; layout < layoutCount; ++layout) {
      const xkb_keysym_t* syms = nullptr;
      const int count = xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &syms);
      if (count < 1 || syms == nullptr) {
        continue;
      }
      const xkb_keysym_t lower = xkb_keysym_to_lower(syms[0]);
      if (lower >= XKB_KEY_a && lower <= XKB_KEY_z) {
        return static_cast<std::uint32_t>(lower);
      }
    }
    return std::nullopt;
  }

  // The key a compositor matches shortcuts against, like smithay's raw_latin_sym_or_raw_current_sym:
  // the unshifted keysym on the active layout (Shift+1 is "1"), or the key's latin letter when the
  // active layout has none (Russian ф is "a"). 0 if the key has no keysym.
  [[nodiscard]] inline std::uint32_t compositorShortcutKeysym(xkb_state* state, xkb_keycode_t keycode) noexcept {
    if (state == nullptr) {
      return 0;
    }
    xkb_keymap* keymap = xkb_state_get_keymap(state);
    const xkb_layout_index_t layout = xkb_state_key_get_layout(state, keycode);
    const xkb_keysym_t* syms = nullptr;
    const int count = xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &syms);
    const xkb_keysym_t raw = (count >= 1 && syms != nullptr) ? syms[0] : XKB_KEY_NoSymbol;
    if (raw != XKB_KEY_NoSymbol && raw < 0x100) {
      return static_cast<std::uint32_t>(xkb_keysym_to_lower(raw));
    }
    return latinShortcutKeysym(keymap, keycode).value_or(static_cast<std::uint32_t>(raw));
  }

} // namespace input
