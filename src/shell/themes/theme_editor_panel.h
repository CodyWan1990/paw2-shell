#pragma once

#include "shell/panel/panel.h"
#include "shell/themes/theme_palette_choices.h"
#include "theme/theme_presets.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class AsyncTextureCache;
class Button;
class ConfigService;
class Flex;
class Image;
class Input;
class Label;
class Renderer;
class ScreenshotService;
class Segmented;
class Select;
struct wl_output;

// Create or edit a saved theme: title, palette source and choice, its wallpapers
// and a preview picture. Open with the theme's id as context, "new" for a fresh
// one, or "resume" to come back to the draft after picking wallpapers or
// capturing the desktop - both of which need this panel out of the way.
class ThemeEditorPanel : public Panel {
public:
  ThemeEditorPanel(
      noctalia::theme::ThemePresetService* presets, ConfigService* config, ScreenshotService* screenshots,
      std::function<wl_output*()> captureOutput, AsyncTextureCache* textures
  );

  void create() override;
  void onOpen(std::string_view context) override;
  void onClose() override;
  [[nodiscard]] bool handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool preedit) override;

  [[nodiscard]] float preferredWidth() const override { return scaled(640.0F); }
  [[nodiscard]] float preferredHeight() const override { return scaled(720.0F); }
  [[nodiscard]] PanelPlacement panelPlacement() const noexcept override { return PanelPlacement::Floating; }
  [[nodiscard]] std::string panelScreenPosition() const override { return "center"; }
  [[nodiscard]] LayerShellKeyboard keyboardMode() const override { return LayerShellKeyboard::Exclusive; }
  [[nodiscard]] bool dismissOnOutsideClick() const override { return false; }
  [[nodiscard]] InputArea* initialFocusArea() const override;

private:
  void doLayout(Renderer& renderer, float width, float height) override;
  void doUpdate(Renderer& renderer) override;
  void syncFromDraft();
  void syncPaletteSelect();
  void rebuildWallpaperList();
  void syncPreview();
  void syncButtons();
  void browseWallpapers();
  void addCurrentWallpaper();
  void captureDesktop();
  void save();
  void requestDelete();
  static void backToThemes();
  static void resumeEditing();

  noctalia::theme::ThemePresetService* m_presets = nullptr;
  ConfigService* m_config = nullptr;
  ScreenshotService* m_screenshots = nullptr;
  std::function<wl_output*()> m_captureOutput;
  AsyncTextureCache* m_textures = nullptr;

  // Kept across close and reopen so picking wallpapers or capturing the
  // desktop does not lose the edit.
  noctalia::theme::ThemePreset m_draft;
  bool m_editingExisting = false;
  bool m_confirmingDelete = false;
  std::vector<themes::PaletteChoice> m_choices;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* rootLayout = nullptr;
    Label* heading = nullptr;
    Input* titleInput = nullptr;
    Segmented* sourceSwitch = nullptr;
    Select* paletteSelect = nullptr;
    Flex* wallpaperList = nullptr;
    Image* preview = nullptr;
    Label* previewNote = nullptr;
    Button* saveButton = nullptr;
    Button* deleteButton = nullptr;
  };
  Ui m_ui;
  struct Thumb {
    Image* image = nullptr;
    std::string path;
  };
  std::vector<Thumb> m_thumbs;
  std::string m_previewPath;
};
