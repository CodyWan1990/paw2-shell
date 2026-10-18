#pragma once

#include "config/config_types.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class IpcService;

namespace noctalia::theme {

  // A saved look: a palette choice plus the wallpapers that go with it. The JSON
  // shape matches the Theme Switcher plugin's themes.json, so its files load as-is.
  struct ThemePreset {
    std::string id;
    std::string title;
    PaletteSource source = PaletteSource::Builtin;
    // Builtin, community or custom palette name; unused for the wallpaper source.
    std::string palette;
    // Generator scheme, used only by the wallpaper source.
    std::string wallpaperScheme = "m3-content";
    std::vector<std::string> wallpapers;
    std::size_t wallpaperIndex = 0;
    std::string screenshotPath;

    // What `color-scheme-set` takes for this preset's source.
    [[nodiscard]] const std::string& paletteValue() const;
    // The wallpaper the preset shows now; empty when it has none.
    [[nodiscard]] std::string currentWallpaper() const;

    bool operator==(const ThemePreset&) const = default;
  };

  class ThemePresetService {
  public:
    // How applying reaches the rest of the shell, so tests can stand in for it.
    struct Hooks {
      std::function<bool(const std::string& path)> setWallpaper;
      std::function<bool(PaletteSource source, std::string_view value)> setColorScheme;
    };

    ThemePresetService(std::filesystem::path storePath, Hooks hooks);

    // Reads the store; a missing or unreadable file is an empty list.
    void load();

    [[nodiscard]] const std::vector<ThemePreset>& presets() const noexcept { return m_presets; }
    [[nodiscard]] const std::string& lastAppliedId() const noexcept { return m_lastAppliedId; }
    [[nodiscard]] const ThemePreset* byId(std::string_view id) const;
    // Exact id, then exact title, then a partial match on either - all ignoring
    // case. An ambiguous partial match finds nothing.
    [[nodiscard]] const ThemePreset* find(std::string_view query) const;
    [[nodiscard]] const ThemePreset* applied() const { return byId(m_lastAppliedId); }

    // Adds or replaces by id; a preset without one gets a fresh slug of its
    // title. Returns the id it was saved under.
    std::string save(ThemePreset preset);
    bool remove(std::string_view id);
    // Shift a preset's place in the list by `delta`.
    bool move(std::string_view id, int delta);

    // Wallpaper first - a wallpaper-sourced palette is generated from it -
    // then the palette.
    bool apply(std::string_view id);
    // The preset `direction` steps from the applied one, wrapping around.
    bool applyAdjacent(int direction);

    // Switch the applied preset's wallpaper without touching its palette.
    bool showWallpaper(std::size_t index);
    bool cycleWallpaper(int direction);
    bool randomWallpaper();

    void addChangeCallback(std::function<void()> callback);
    void registerIpc(IpcService& ipc);

    [[nodiscard]] std::string uniqueSlug(std::string_view title) const;

  private:
    ThemePreset* mutableById(std::string_view id);
    void persist() const;
    void notify() const;

    std::filesystem::path m_storePath;
    Hooks m_hooks;
    std::vector<ThemePreset> m_presets;
    std::string m_lastAppliedId;
    std::vector<std::function<void()>> m_changeCallbacks;
  };

} // namespace noctalia::theme
