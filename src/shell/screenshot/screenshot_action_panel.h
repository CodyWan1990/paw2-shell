#pragma once

#include "capture/screenshot_service.h"
#include "shell/panel/panel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class Button;
class Flex;
class Image;
class Input;
class InputArea;
class Label;
class Renderer;

// paw2-shell's screenshot tool: after a capture, a small centred panel with the preview, a file
// name (blank uses the dated default) and Copy / Save / Both. Esc discards, Tab cycles the
// buttons, Enter runs the highlighted one.
class ScreenshotActionPanel : public Panel {
public:
  explicit ScreenshotActionPanel(ScreenshotService* screenshots);

  void create() override;
  void onOpen(std::string_view context) override;
  void onClose() override;

  [[nodiscard]] float preferredWidth() const override { return scaled(460.0F); }
  [[nodiscard]] float preferredHeight() const override;
  [[nodiscard]] PanelPlacement panelPlacement() const noexcept override { return PanelPlacement::Floating; }
  [[nodiscard]] std::string panelScreenPosition() const override { return "center"; }
  [[nodiscard]] LayerShellLayer layer() const override { return LayerShellLayer::Overlay; }
  [[nodiscard]] LayerShellKeyboard keyboardMode() const override { return LayerShellKeyboard::Exclusive; }
  [[nodiscard]] bool dismissOnOutsideClick() const override { return false; }
  [[nodiscard]] InputArea* initialFocusArea() const override;
  [[nodiscard]] bool handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool preedit) override;

private:
  using Action = ScreenshotService::PromptAction;

  void onPanelCardOpacityChanged(float opacity) override;
  void doLayout(Renderer& renderer, float width, float height) override;
  void doUpdate(Renderer& renderer) override;
  void select(std::size_t index);
  void run(Action action);
  void discard();
  bool handleInputKey(std::uint32_t sym, std::uint32_t modifiers);

  ScreenshotService* m_screenshots = nullptr;
  Flex* m_rootLayout = nullptr;
  Image* m_preview = nullptr;
  Input* m_nameInput = nullptr;
  std::array<Button*, 3> m_buttons{};
  std::size_t m_selected = 0;
  bool m_previewDirty = false;
  bool m_answered = false;
};
