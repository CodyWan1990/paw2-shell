#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace paw2shell::theme {

  struct GeneratedPalette;

  // A palette stored as a JSON file, with the swatches pickers show for it.
  struct AvailablePalette {
    struct PreviewMode {
      std::string surface;
      std::vector<std::string> accents;
    };

    struct Preview {
      PreviewMode dark;
      PreviewMode light;
    };

    std::string name;
    Preview preview = {};
  };

  // Presets that ship with the shell (assets/palettes), read through the builtin source
  // alongside the palettes compiled into builtin_palettes.cpp.
  [[nodiscard]] std::filesystem::path presetPalettePath(std::string_view name);
  [[nodiscard]] bool presetPaletteExists(std::string_view name);
  [[nodiscard]] std::vector<AvailablePalette> availablePresetPalettes();

  [[nodiscard]] std::filesystem::path customPaletteDir();
  [[nodiscard]] std::filesystem::path customPalettePath(std::string_view name);
  [[nodiscard]] std::vector<AvailablePalette> availableCustomPalettes();

  [[nodiscard]] std::string suggestCustomPaletteName(std::string_view wallpaperPath, std::string_view scheme);
  [[nodiscard]] std::string allocateCustomPaletteName(std::string_view wallpaperPath, std::string_view scheme);
  [[nodiscard]] bool saveCustomPaletteFromGenerated(
      std::string_view name, const GeneratedPalette& palette, std::string* errorOut = nullptr
  );

} // namespace paw2shell::theme
