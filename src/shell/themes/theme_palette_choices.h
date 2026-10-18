#pragma once

#include "config/config_types.h"
#include "ui/controls/color_swatch_preview.h"

#include <string>
#include <vector>

namespace noctalia::theme {
  struct ThemePreset;
}

namespace themes {

  // One palette a preset can use, as the editor lists it.
  struct PaletteChoice {
    std::string value;
    std::string label;
    ColorSwatchPreview preview;
  };

  // The palettes `source` offers - builtin, community or custom names, or the
  // wallpaper generator's schemes - with swatches for `mode` where known.
  [[nodiscard]] std::vector<PaletteChoice> paletteChoices(PaletteSource source, ThemeMode mode);

  // The swatches a preset's card shows; empty for a wallpaper-generated palette,
  // whose colours depend on the image.
  [[nodiscard]] ColorSwatchPreview presetPreview(const noctalia::theme::ThemePreset& preset, ThemeMode mode);

} // namespace themes
