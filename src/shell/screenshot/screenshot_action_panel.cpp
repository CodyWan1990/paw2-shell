#include "shell/screenshot/screenshot_action_panel.h"

#include "core/input/key_modifiers.h"
#include "core/input/keybind_matcher.h"
#include "i18n/i18n.h"
#include "render/core/renderer.h"
#include "render/core/texture_manager.h"
#include "shell/panel/panel_manager.h"
#include "shell/panel/panel_scaffold.h"
#include "ui/builders.h"
#include "ui/controls/image.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <cmath>
#include <memory>
#include <string>

namespace {

  constexpr float kPreviewHeight = 180.0F;

  struct ActionButton {
    ScreenshotService::PromptAction action;
    const char* labelKey;
    const char* glyph;
  };

  constexpr std::array<ActionButton, 3> kActions{{
      {ScreenshotService::PromptAction::Copy, "bar.screenshot.action-copy", "copy"},
      {ScreenshotService::PromptAction::Save, "bar.screenshot.action-save", "device-floppy"},
      {ScreenshotService::PromptAction::Both, "bar.screenshot.action-both", "files"},
  }};

} // namespace

ScreenshotActionPanel::ScreenshotActionPanel(ScreenshotService* screenshots) : m_screenshots(screenshots) {}

float ScreenshotActionPanel::preferredHeight() const {
  const float scale = contentScale();
  const float pad = Style::spaceLg * scale;
  const float gap = Style::spaceMd * scale;
  const float title = Style::fontSizeTitle * scale * 1.35F;
  const float caption = Style::fontSizeCaption * scale * 1.35F;
  const float control = Style::controlHeight * scale;
  return std::ceil(
      pad * 2.0F + title + scaled(kPreviewHeight) + control * 2.0F + caption + gap * 4.0F
      + scaled(Style::panelPadding) * 2.0F
  );
}

void ScreenshotActionPanel::create() {
  const float scale = contentScale();
  auto root = panel_scaffold::rootColumn(&m_rootLayout, scale);
  root->addChild(panel_scaffold::header("camera", i18n::tr("bar.screenshot.prompt-title"), scale));

  root->addChild(ui::image({
      .out = &m_preview,
      .fit = ImageFit::Contain,
      .radius = Style::radiusSm * scale,
      .height = scaled(kPreviewHeight),
  }));

  root->addChild(ui::input({
      .out = &m_nameInput,
      .surfaceOpacity = panelCardOpacity(),
      .onSubmit = [this](const std::string& /*value*/) { run(kActions[m_selected].action); },
      .onKeyEvent = [this](std::uint32_t sym, std::uint32_t modifiers) { return handleInputKey(sym, modifiers); },
  }));

  auto buttons = ui::row({
      .align = FlexAlign::Center,
      .justify = FlexJustify::Center,
      .gap = Style::spaceSm * scale,
      .fillWidth = true,
  });
  for (std::size_t i = 0; i < kActions.size(); ++i) {
    buttons->addChild(
        ui::button({
            .out = &m_buttons[i],
            .text = i18n::tr(kActions[i].labelKey),
            .glyph = kActions[i].glyph,
            .variant = i == m_selected ? ButtonVariant::Primary : ButtonVariant::Outline,
            .onClick = [this, i]() { run(kActions[i].action); },
        })
    );
  }
  root->addChild(std::move(buttons));

  root->addChild(panel_scaffold::hint(i18n::tr("bar.screenshot.prompt-hint"), scale));

  setRoot(std::move(root));
}

void ScreenshotActionPanel::onOpen(std::string_view /*context*/) {
  m_answered = false;
  m_previewDirty = true;
  select(0);
  if (m_nameInput != nullptr) {
    m_nameInput->setValue("");
    m_nameInput->setPlaceholder(m_screenshots != nullptr ? m_screenshots->pendingPromptDefaultName() : "");
  }
}

void ScreenshotActionPanel::onClose() {
  // Closed without an answer (the panel was replaced or closed some other way): drop the capture
  // rather than leave it waiting with nothing on screen to answer it.
  if (!m_answered && m_screenshots != nullptr) {
    m_screenshots->discardPrompt();
  }
  clearReleasedRoot();
  m_rootLayout = nullptr;
  m_preview = nullptr;
  m_nameInput = nullptr;
  m_buttons = {};
}

InputArea* ScreenshotActionPanel::initialFocusArea() const {
  return m_nameInput != nullptr ? m_nameInput->inputArea() : nullptr;
}

void ScreenshotActionPanel::select(std::size_t index) {
  m_selected = index % kActions.size();
  for (std::size_t i = 0; i < m_buttons.size(); ++i) {
    if (m_buttons[i] != nullptr) {
      m_buttons[i]->setVariant(i == m_selected ? ButtonVariant::Primary : ButtonVariant::Outline);
    }
  }
}

void ScreenshotActionPanel::run(Action action) {
  if (m_screenshots == nullptr || m_answered) {
    return;
  }
  m_answered = true;
  const std::string name = m_nameInput != nullptr ? m_nameInput->value() : std::string{};
  // Close first: the capture is delivered from the service, and the panel must not hold
  // a frame of the desktop on screen while the file is written.
  PanelManager::instance().close();
  m_screenshots->completePrompt(action, name);
}

void ScreenshotActionPanel::discard() {
  if (m_screenshots != nullptr) {
    m_screenshots->discardPrompt();
  }
  m_answered = true;
  PanelManager::instance().close();
}

bool ScreenshotActionPanel::handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool /*preedit*/) {
  if (!pressed) {
    return false;
  }
  if (KeybindMatcher::matches(KeybindAction::Cancel, sym, modifiers)) {
    discard();
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::TabNext, sym, modifiers)) {
    select(m_selected + 1);
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::TabPrevious, sym, modifiers)) {
    select(m_selected + kActions.size() - 1);
    return true;
  }
  return false;
}

bool ScreenshotActionPanel::handleInputKey(std::uint32_t sym, std::uint32_t modifiers) {
  if (KeybindMatcher::matches(KeybindAction::Validate, sym, modifiers)) {
    run(kActions[m_selected].action);
    return true;
  }
  const bool shift = (modifiers & KeyMod::Shift) != 0;
  if (KeybindMatcher::matches(KeybindAction::Left, sym, modifiers)) {
    m_nameInput->moveCaretLeft(shift);
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::Right, sym, modifiers)) {
    m_nameInput->moveCaretRight(shift);
    return true;
  }
  return handleGlobalKey(sym, modifiers, true, false);
}

void ScreenshotActionPanel::onPanelCardOpacityChanged(float opacity) {
  if (m_nameInput != nullptr) {
    m_nameInput->setSurfaceOpacity(opacity);
  }
}

void ScreenshotActionPanel::doUpdate(Renderer& renderer) {
  if (!m_previewDirty || m_preview == nullptr || m_screenshots == nullptr) {
    return;
  }
  m_previewDirty = false;
  if (const ScreencopyImage* image = m_screenshots->pendingPromptImage();
      image != nullptr && image->width > 0 && image->height > 0) {
    (void)m_preview->setSourceRaw(
        renderer, image->rgba.data(), image->rgba.size(), image->width, image->height, image->width * 4,
        PixmapFormat::RGBA, true
    );
  } else {
    m_preview->clear(renderer);
  }
}

void ScreenshotActionPanel::doLayout(Renderer& renderer, float width, float height) {
  if (m_rootLayout == nullptr) {
    return;
  }
  doUpdate(renderer);
  m_rootLayout->setSize(width, height);
  m_rootLayout->layout(renderer);
}
