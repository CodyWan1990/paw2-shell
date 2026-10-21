#pragma once

#include "core/timer_manager.h"
#include "render/core/color.h"
#include "render/core/thumbnail_service.h"
#include "shell/panel/panel.h"
#include "shell/themes/theme_palette_choices.h"
#include "shell/wallpaper/panel/wallpaper_scanner.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

class Button;
class ConfigService;
class Flex;
class Input;
class Label;
class Segmented;
class Select;
class Spinner;
class Toggle;
class VirtualGridView;
class WallpaperGridAdapter;
class WaylandConnection;

namespace paw2shell::theme {
  class ThemeService;
}

class WallpaperPanel : public Panel {
public:
  enum class SortMode : std::uint8_t {
    NameAsc,
    NameDesc,
    DateAsc,
    DateDesc,
    Random,
  };

  WallpaperPanel(
      WaylandConnection* wayland, ConfigService* config, ThumbnailService* thumbnails, WallpaperScanner* scanner,
      paw2shell::theme::ThemeService* themeService = nullptr
  );

  // Chosen wallpaper paths, or none when the choice was cancelled.
  using PickCallback = std::function<void(std::vector<std::string>)>;
  // The next open with context "pick" lets the user tick any number of wallpapers
  // for `done` instead of applying one.
  static void requestPick(PickCallback done);
  ~WallpaperPanel() override;

  void create() override;
  void onOpen(std::string_view context) override;
  void onClose() override;
  [[nodiscard]] bool handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool preedit) override;

  [[nodiscard]] float preferredWidth() const override { return scaled(980.0F); }
  [[nodiscard]] float preferredHeight() const override { return scaled(700.0F); }
  [[nodiscard]] PanelPlacement panelPlacement() const noexcept override;
  [[nodiscard]] LayerShellKeyboard keyboardMode() const override { return LayerShellKeyboard::Exclusive; }
  [[nodiscard]] InputArea* initialFocusArea() const override;

private:
  void doLayout(Renderer& renderer, float width, float height) override;
  void doUpdate(Renderer& renderer) override;
  void onPanelCardOpacityChanged(float opacity) override;
  // "ALL" is represented by an empty connector string.
  struct MonitorChoice {
    std::string connector; // empty = ALL
    std::string label;
  };

  void populateMonitorChoices();
  void refreshVisibleEntries();
  void refreshScan();
  void onScanComplete();
  void syncLoadingState();
  void applyFilter();
  void syncBrowseChrome();
  void syncBackButton();
  void navigateInto(const std::filesystem::path& dir);
  void navigateUp();
  void applyWallpaperFromEntry(const WallpaperEntry& entry);
  void applyWallpaperPath(const std::string& path, const WallpaperFavorite* applyTheme);
  [[nodiscard]] const WallpaperFavorite* favoriteThemeToApply(std::string_view path) const;
  [[nodiscard]] WallpaperFavorite themeFromControls() const;
  [[nodiscard]] WallpaperFavorite activeThemeSettings() const;
  void applyThemeFromControls();
  void toggleFavoriteForPath(const std::string& path);
  void syncThemeControls();
  void rebuildFavoritePaletteDetailSelect(const WallpaperFavorite* favorite);
  [[nodiscard]] std::optional<PaletteSource> paletteSourceForSegment(std::size_t index) const;
  [[nodiscard]] std::size_t segmentForPaletteSource(PaletteSource source) const;
  [[nodiscard]] std::string selectedWallpaperPath() const;
  void appendFilteredFavoriteEntries(
      std::vector<WallpaperEntry>& out, std::unordered_set<std::string>& favoritePaths,
      const std::filesystem::path& activeDir, const std::filesystem::path& rootDir
  ) const;
  void sortVisibleEntries();
  void reseedRandomSort();
  void syncSortButtonGlyph();
  void cycleSortMode();
  void setSortMode(SortMode mode);
  void applyColorWallpaper();
  void rebindGrid(bool resetScroll = false);
  void resetSelection();
  [[nodiscard]] bool hasVisibleSelection() const;
  void selectVisibleIndex(std::size_t index);
  void activateSelectedEntry();
  [[nodiscard]] bool handleKeyEvent(std::uint32_t sym, std::uint32_t modifiers);
  [[nodiscard]] std::filesystem::path activeDirectoryForSelection() const;
  [[nodiscard]] std::filesystem::path rootDirectoryForSelection() const;
  [[nodiscard]] std::string currentWallpaperPathForSelection() const;
  [[nodiscard]] std::vector<std::string> allMonitorConnectors() const;
  [[nodiscard]] std::optional<Color> selectedFillColor() const;
  void togglePicked(const std::string& path);
  void finishPick(bool accept);
  void syncPickBar();
  [[nodiscard]] static std::string displayNameForWallpaperPath(std::string_view path);

  WaylandConnection* m_wayland = nullptr;
  ConfigService* m_config = nullptr;
  ThumbnailService* m_thumbnails = nullptr;
  WallpaperScanner* m_scanner = nullptr;
  paw2shell::theme::ThemeService* m_themeService = nullptr;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    // UI nodes (owned by the root flex tree).
    Flex* rootLayout = nullptr;
    Button* backButton = nullptr;
    Segmented* favoriteThemeSegmented = nullptr;
    Segmented* favoritePaletteSourceSegmented = nullptr;
    Select* favoritePaletteDetailSelect = nullptr;
    Select* monitorSelect = nullptr;
    Input* filterInput = nullptr;
    Button* favoriteCurrentButton = nullptr;
    Toggle* flattenToggle = nullptr;
    Button* sortButton = nullptr;
    VirtualGridView* grid = nullptr;
    Flex* loadingBox = nullptr;
    Spinner* spinner = nullptr;
    Flex* pickBar = nullptr;
    Label* pickCount = nullptr;
    Button* pickAddButton = nullptr;
  };
  Ui m_ui;
  std::unique_ptr<WallpaperGridAdapter> m_adapter;

  // Pick mode: tiles toggle in and out of m_picked instead of applying.
  static inline PickCallback s_pendingPick;
  PickCallback m_pickDone;
  std::set<std::string> m_picked;
  bool m_picking = false;

  std::vector<MonitorChoice> m_monitorChoices;
  std::size_t m_selectedMonitorIndex = 0;

  // Navigation state for the current selected monitor.
  std::vector<std::filesystem::path> m_navStack;

  // Filtered view of the scanner's entries for the currently active
  // directory. The grid reads a page-sized slice of this vector via a
  // raw pointer; any mutation of the vector must be followed by applyPage().
  std::vector<WallpaperEntry> m_visibleEntries;

  std::string m_filterQuery;
  std::string m_pendingFilterQuery;
  Timer m_filterDebounceTimer;

  bool m_flatten = false;
  bool m_showNames = true;
  bool m_scanPending = false;
  SortMode m_sortMode = SortMode::NameAsc;
  std::uint64_t m_randomSeed = 0;
  std::size_t m_pinnedFavoriteCount = 0;
  bool m_syncingFavoriteControls = false;
  bool m_syncingGridSelectionVisual = false;
  bool m_gridKeyboardActive = false;
  std::vector<themes::PaletteChoice> m_favoritePaletteDetailValues;
  std::vector<PaletteSource> m_paletteSourceOrder;
  static constexpr std::size_t kNoVisibleSelection = std::numeric_limits<std::size_t>::max();
  std::size_t m_selectedVisibleIndex = kNoVisibleSelection;
  float m_lastWidth = 0.0F;
  float m_lastHeight = 0.0F;
  bool m_dirty = false;
  bool m_thumbnailRefreshPending = false;
  ThumbnailService::Subscription m_thumbnailPendingSub;
};
