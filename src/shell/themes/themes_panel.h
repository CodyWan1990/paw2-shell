#pragma once

#include "shell/panel/panel.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class AsyncTextureCache;
class ConfigService;
class Flex;
class Image;
class Renderer;
class ScrollView;
class Segmented;
class Button;

namespace noctalia::theme {
  class ThemePresetService;
}

// Saved themes as a strip of cards: click one to apply its wallpaper and palette.
// A second view shows the applied theme's wallpapers, to switch between them
// without touching the palette. Editing happens in ThemeEditorPanel.
class ThemesPanel : public Panel {
public:
  ThemesPanel(noctalia::theme::ThemePresetService* presets, ConfigService* config, AsyncTextureCache* textures);

  void create() override;
  void onOpen(std::string_view context) override;
  void onClose() override;
  [[nodiscard]] bool handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool preedit) override;

  [[nodiscard]] float preferredWidth() const override { return scaled(920.0F); }
  [[nodiscard]] float preferredHeight() const override { return scaled(380.0F); }
  [[nodiscard]] PanelPlacement panelPlacement() const noexcept override { return PanelPlacement::Floating; }
  [[nodiscard]] std::string panelScreenPosition() const override { return "center"; }
  [[nodiscard]] LayerShellKeyboard keyboardMode() const override { return LayerShellKeyboard::Exclusive; }

private:
  enum class View : std::uint8_t { Themes, Wallpapers };

  // A card's picture, loaded off the UI thread.
  struct CardImage {
    Image* image = nullptr;
    std::string path;
  };

  void doLayout(Renderer& renderer, float width, float height) override;
  void doUpdate(Renderer& renderer) override;
  void rebuildCards();
  void addThemeCards();
  void addWallpaperCards();
  std::unique_ptr<Node> makeCard(
      std::string_view imagePath, bool current, bool selected, std::unique_ptr<Node> caption,
      std::function<void()> onClick, std::function<void()> onEdit = nullptr
  );
  void activate(std::size_t index);
  void select(std::size_t index);
  void setView(View view);
  static void openEditor(std::string_view id);

  noctalia::theme::ThemePresetService* m_presets = nullptr;
  ConfigService* m_config = nullptr;
  AsyncTextureCache* m_textures = nullptr;

  Flex* m_rootLayout = nullptr;
  ScrollView* m_scroll = nullptr;
  Segmented* m_viewSwitch = nullptr;
  Button* m_randomButton = nullptr;
  std::vector<Node*> m_cards;
  std::vector<CardImage> m_images;
  View m_view = View::Themes;
  std::size_t m_selected = 0;
  bool m_dirty = false;
};
