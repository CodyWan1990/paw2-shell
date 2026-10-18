#include "shell/themes/theme_palette_choices.h"

#include "i18n/i18n.h"
#include "theme/builtin_palettes.h"
#include "theme/community_palettes.h"
#include "theme/custom_palettes.h"
#include "theme/theme_presets.h"
#include "ui/palette.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace themes {

  namespace {

    constexpr std::array<std::string_view, 10> kWallpaperSchemes{
        "m3-content", "m3-tonal-spot", "m3-fruit-salad", "m3-rainbow", "m3-monochrome",
        "vibrant",    "faithful",      "soft",           "dysfunctional", "muted",
    };

    ColorSwatchPreview previewOf(const ::Palette& palette) {
      return ColorSwatchPreview{
          .surface = fixedColorSpec(palette.surface),
          .swatches = {
              fixedColorSpec(palette.primary),
              fixedColorSpec(palette.secondary),
              fixedColorSpec(palette.tertiary),
              fixedColorSpec(palette.error),
          },
      };
    }

    ColorSwatchPreview previewOf(const noctalia::theme::AvailablePalette::PreviewMode& metadata) {
      ColorSwatchPreview preview;
      if (Color surface; tryParseHexColor(metadata.surface, surface)) {
        preview.surface = fixedColorSpec(surface);
      }
      for (const auto& hex : metadata.accents) {
        if (Color accent; tryParseHexColor(hex, accent)) {
          preview.swatches.push_back(fixedColorSpec(accent));
        }
      }
      return preview;
    }

    ColorSwatchPreview previewOf(const noctalia::theme::AvailablePalette& palette, ThemeMode mode) {
      if (mode == ThemeMode::Light) {
        if (auto light = previewOf(palette.preview.light); !light.empty()) {
          return light;
        }
      }
      return previewOf(palette.preview.dark);
    }

    std::vector<PaletteChoice> fromAvailable(const std::vector<noctalia::theme::AvailablePalette>& palettes, ThemeMode mode) {
      std::vector<PaletteChoice> choices;
      choices.reserve(palettes.size());
      for (const auto& palette : palettes) {
        choices.push_back({.value = palette.name, .label = palette.name, .preview = previewOf(palette, mode)});
      }
      return choices;
    }

  } // namespace

  std::vector<PaletteChoice> paletteChoices(PaletteSource source, ThemeMode mode) {
    switch (source) {
    case PaletteSource::Builtin: {
      std::vector<PaletteChoice> choices;
      for (const auto& palette : noctalia::theme::builtinPalettes()) {
        choices.push_back({
            .value = std::string(palette.name),
            .label = std::string(palette.name),
            .preview = previewOf(mode == ThemeMode::Light ? palette.light.palette : palette.dark.palette),
        });
      }
      return choices;
    }
    case PaletteSource::Wallpaper: {
      std::vector<PaletteChoice> choices;
      for (const auto scheme : kWallpaperSchemes) {
        choices.push_back({.value = std::string(scheme), .label = i18n::tr("theme.scheme." + std::string(scheme)), .preview = {}});
      }
      return choices;
    }
    case PaletteSource::Community:
      return fromAvailable(noctalia::theme::availableCommunityPalettes(), mode);
    case PaletteSource::Custom:
      return fromAvailable(noctalia::theme::availableCustomPalettes(), mode);
    }
    return {};
  }

  ColorSwatchPreview presetPreview(const noctalia::theme::ThemePreset& preset, ThemeMode mode) {
    if (preset.source == PaletteSource::Wallpaper) {
      return {};
    }
    const auto choices = paletteChoices(preset.source, mode);
    const auto it = std::ranges::find(choices, preset.palette, &PaletteChoice::value);
    return it != choices.end() ? it->preview : ColorSwatchPreview{};
  }

} // namespace themes
