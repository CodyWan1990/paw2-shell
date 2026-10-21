#include "shell/polkit/polkit_panel.h"

#include "config/config_service.h"
#include "config/config_types.h"
#include "core/files/resource_paths.h"
#include "core/input/key_modifiers.h"
#include "core/input/keybind_matcher.h"
#include "dbus/polkit/polkit_agent.h"
#include "i18n/i18n.h"
#include "render/core/renderer.h"
#include "render/scene/input_area.h"
#include "shell/panel/panel_manager.h"
#include "ui/builders.h"
#include "ui/controls/glyph.h"
#include "ui/controls/image.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>

namespace {

  int wrappedLineCount(std::string_view text, int charsPerLine, int maxLines) {
    if (text.empty()) {
      return 0;
    }
    int lines = 0;
    int col = 0;
    for (char ch : text) {
      if (ch == '\n') {
        ++lines;
        col = 0;
        if (lines >= maxLines) {
          return maxLines;
        }
        continue;
      }
      ++col;
      if (charsPerLine > 0 && col > charsPerLine) {
        ++lines;
        col = 1;
        if (lines >= maxLines) {
          return maxLines;
        }
      }
    }
    if (col > 0 || lines == 0) {
      ++lines;
    }
    return std::min(lines, maxLines);
  }

  std::string wrapLongRuns(std::string text, std::size_t maxRun = 48) {
    std::string out;
    out.reserve(text.size() + text.size() / maxRun);
    std::size_t run = 0;
    for (char ch : text) {
      const bool breakable = std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '/' || ch == ':' || ch == '-';
      out.push_back(ch);
      if (breakable) {
        run = 0;
        continue;
      }
      ++run;
      if (run >= maxRun) {
        out.push_back('\n');
        run = 0;
      }
    }
    return out;
  }

} // namespace

PolkitPanel::PolkitPanel(ConfigService* config, std::function<PolkitAgent*()> agentProvider)
    : m_config(config), m_agentProvider(std::move(agentProvider)) {}

PanelPlacement PolkitPanel::panelPlacement() const noexcept {
  return m_config != nullptr ? m_config->config().shell.panel.polkitPlacement : PanelPlacement::Floating;
}

float PolkitPanel::preferredHeight() const {
  const float scale = contentScale();
  const float bodyLine = Style::fontSizeBody * scale * 1.35F;
  const float titleLine = Style::fontSizeTitle * scale * 1.35F;
  const float captionLine = Style::fontSizeCaption * scale * 1.35F;
  const float iconSize = scaled(48.0F);
  const float pad = Style::spaceLg * scale;
  const float gapMd = Style::spaceMd * scale;
  const float gapSm = Style::spaceSm * scale;

  const float contentW = preferredWidth() - scaled(Style::panelPadding) * 2.0F;
  const float innerW = std::max(1.0F, contentW - pad * 2.0F);
  const float messageW = std::max(1.0F, innerW - iconSize - gapMd);
  const float avgChar = Style::fontSizeBody * scale * 0.55F;
  const int messageChars = std::max(1, static_cast<int>(messageW / avgChar));
  const int promptChars = std::max(1, static_cast<int>(innerW / avgChar));

  int messageLines = 1;
  int promptLines = 1;
  int supplementaryLines = 0;
  bool needsInput = false;

  if (PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
      agent != nullptr && agent->hasPendingRequest()) {
    needsInput = agent->isResponseRequired();
    const PolkitRequest request = agent->pendingRequest();
    const std::string message = wrapLongRuns(request.message.empty() ? request.actionId : request.message);
    messageLines = std::max(1, wrappedLineCount(message, messageChars, 6));

    const std::string supplementaryRaw = agent->supplementaryMessage();
    const bool supplementaryError = agent->supplementaryIsError();
    std::string promptText = wrapLongRuns(agent->inputPrompt());
    std::string supplementaryText = wrapLongRuns(supplementaryRaw);
    if (!needsInput && !supplementaryText.empty() && !supplementaryError) {
      promptText = supplementaryText;
      supplementaryText.clear();
    } else if (
        !supplementaryText.empty()
        && (supplementaryError || supplementaryText == i18n::tr("auth.polkit.authenticating"))
    ) {
      promptText = supplementaryText;
      supplementaryText.clear();
    }
    promptLines = std::max(1, wrappedLineCount(promptText, promptChars, 3));
    supplementaryLines = wrappedLineCount(supplementaryText, promptChars, 4);
  }

  const float top = std::max(iconSize, titleLine + static_cast<float>(messageLines) * bodyLine);
  float bottom = static_cast<float>(promptLines) * bodyLine + gapSm;
  if (needsInput) {
    bottom += Style::controlHeight * scale + gapSm;
  }
  if (supplementaryLines > 0) {
    bottom += static_cast<float>(supplementaryLines) * captionLine + gapSm;
  }
  bottom += Style::controlHeight * scale;

  return std::ceil(pad * 2.0F + top + gapMd + bottom + scaled(Style::panelPadding) * 2.0F + gapSm);
}

void PolkitPanel::create() {
  const float scale = contentScale();
  const float iconSize = scaled(48.0F);
  auto root = ui::column({
      .out = &m_ui.rootLayout,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
      .padding = Style::spaceLg * scale,
  });

  auto focusArea = ui::inputArea({});
  focusArea->setFocusable(true);
  focusArea->setVisible(false);
  m_ui.focusArea = static_cast<InputArea*>(root->addChild(std::move(focusArea)));

  auto iconContainer = ui::node({
      .out = &m_ui.iconContainer,
      .width = iconSize,
      .height = iconSize,
  });
  auto iconFallback = ui::glyph({
      .out = &m_ui.fallbackIcon,
      .glyph = "shield-lock",
      .glyphSize = iconSize * 0.65F,
      .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
  });
  iconContainer->addChild(std::move(iconFallback));
  auto iconImage = ui::image({
      .out = &m_ui.icon,
      .fit = ImageFit::Contain,
      .visible = false,
  });
  iconContainer->addChild(std::move(iconImage));

  auto topContent = ui::row(
      {.align = FlexAlign::Center, .gap = Style::spaceMd * scale}, std::move(iconContainer),
      ui::column(
          {.align = FlexAlign::Stretch, .flexGrow = 1.0F},
          ui::label({
              .text = i18n::tr("auth.polkit.title"),
              .fontSize = Style::fontSizeTitle * scale,
              .fontWeight = FontWeight::Bold,
              .color = colorSpecFromRole(ColorRole::Primary),
          }),
          ui::label({
              .out = &m_ui.messageLabel,
              .fontSize = Style::fontSizeBody * scale,
              .color = colorSpecFromRole(ColorRole::OnSurface),
              .maxLines = 6,
          })
      )
  );
  root->addChild(std::move(topContent));

  auto bottomContent = ui::column(
      {.align = FlexAlign::Stretch, .gap = Style::spaceSm * scale},
      ui::label({
          .out = &m_ui.promptLabel,
          .fontSize = Style::fontSizeBody * scale,
          .color = colorSpecFromRole(ColorRole::OnSurface),
          .maxLines = 3,
      }),
      ui::input({
          .out = &m_ui.input,
          .placeholder = i18n::tr("auth.polkit.password-placeholder"),
          .passwordMode = true,
          .surfaceOpacity = panelCardOpacity(),
          .onChange =
              [this](const std::string& value) {
                if (m_ui.submitButton != nullptr) {
                  m_ui.submitButton->setEnabled(m_lastResponseRequired && !value.empty());
                }
              },
          .onSubmit = [this](const std::string& value) { submit(value); },
          .onKeyEvent =
              [this](std::uint32_t sym, std::uint32_t modifiers) { return handleInputKeyEvent(sym, modifiers); },
      }),
      ui::label({
          .out = &m_ui.supplementaryLabel,
          .fontSize = Style::fontSizeCaption * scale,
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
          .maxLines = 4,
      }),
      ui::row(
          {
              .align = FlexAlign::Center,
              .justify = FlexJustify::End,
              .wrap = true,
              .gap = Style::spaceSm * scale,
              .fillWidth = true,
          },
          ui::button({
              .text = i18n::tr("common.actions.cancel"),
              .variant = ButtonVariant::Outline,
              .onClick =
                  [this]() {
                    cancelAuth();
                    PanelManager::instance().close();
                  },
          }),
          ui::button({
              .out = &m_ui.submitButton,
              .text = i18n::tr("auth.polkit.authenticate"),
              .variant = ButtonVariant::Primary,
              .onClick = [this]() { submit(); },
          })
      )
  );
  root->addChild(std::move(bottomContent));
  setRoot(std::move(root));
}

void PolkitPanel::onOpen(std::string_view /*context*/) {
  m_lastResponseRequired = false;
  m_iconResolved = false;
  m_hasTrackedRequest = false;
  m_trackedRequestCookie.clear();
  if (PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
      agent != nullptr && agent->hasPendingRequest()) {
    m_trackedRequestCookie = agent->pendingRequest().cookie;
    m_hasTrackedRequest = true;
  }
  if (m_ui.input != nullptr) {
    m_ui.input->setValue("");
  }
}

void PolkitPanel::onClose() {
  // A panel close must settle the request it displayed. Match the cookie so a
  // chained authentication that arrived after a successful response survives.
  if (m_hasTrackedRequest) {
    if (PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
        agent != nullptr && agent->hasPendingRequest() && agent->pendingRequest().cookie == m_trackedRequestCookie) {
      agent->cancelRequest();
    }
  }
  m_hasTrackedRequest = false;
  m_trackedRequestCookie.clear();
  m_lastResponseRequired = false;
  clearReleasedRoot();

  m_ui = {};
}

void PolkitPanel::cancelAuth() {
  PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
  if (agent != nullptr && agent->hasPendingRequest()) {
    agent->cancelRequest();
  }
}

bool PolkitPanel::handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool /*preedit*/) {
  if (!pressed || !KeybindMatcher::matches(KeybindAction::Cancel, sym, modifiers)) {
    return false;
  }
  cancelAuth();
  PanelManager::instance().close();
  return true;
}

InputArea* PolkitPanel::initialFocusArea() const {
  PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
  if (agent != nullptr && !agent->isResponseRequired()) {
    return m_ui.focusArea;
  }
  return m_ui.input != nullptr ? m_ui.input->inputArea() : m_ui.focusArea;
}

void PolkitPanel::doLayout(Renderer& renderer, float width, float height) {
  if (m_ui.rootLayout == nullptr) {
    return;
  }
  m_ui.rootLayout->setSize(width, height);
  m_ui.rootLayout->layout(renderer);
  if (m_ui.iconContainer != nullptr) {
    if (m_ui.icon != nullptr && m_ui.icon->visible()) {
      m_ui.icon->setSize(m_ui.iconContainer->width(), m_ui.iconContainer->height());
      m_ui.icon->setPosition(0.0F, 0.0F);
    }
    if (m_ui.fallbackIcon != nullptr && m_ui.fallbackIcon->visible()) {
      const float ox = std::round((m_ui.iconContainer->width() - m_ui.fallbackIcon->width()) * 0.5F);
      const float oy = std::round((m_ui.iconContainer->height() - m_ui.fallbackIcon->height()) * 0.5F);
      m_ui.fallbackIcon->setPosition(ox, oy);
    }
  }
}

void PolkitPanel::doUpdate(Renderer& renderer) {
  PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
  if (agent == nullptr
      || m_ui.messageLabel == nullptr
      || m_ui.promptLabel == nullptr
      || m_ui.supplementaryLabel == nullptr
      || m_ui.submitButton == nullptr
      || m_ui.input == nullptr
      || m_ui.icon == nullptr
      || m_ui.fallbackIcon == nullptr) {
    return;
  }
  const PolkitRequest request = agent->pendingRequest();
  if (agent->hasPendingRequest()) {
    m_trackedRequestCookie = request.cookie;
    m_hasTrackedRequest = true;
  }
  const bool needsInput = agent->isResponseRequired();
  const std::string supplementaryRaw = agent->supplementaryMessage();
  const bool supplementaryError = agent->supplementaryIsError();
  const bool isInvalidPassword = supplementaryError && supplementaryRaw == i18n::tr("auth.polkit.invalid-password");
  std::string promptText = wrapLongRuns(agent->inputPrompt());
  std::string supplementaryText = wrapLongRuns(supplementaryRaw);
  if (!needsInput && !supplementaryText.empty() && !supplementaryError) {
    promptText = supplementaryText;
    supplementaryText.clear();
  } else if (
      !supplementaryText.empty() && (supplementaryError || supplementaryText == i18n::tr("auth.polkit.authenticating"))
  ) {
    promptText = supplementaryText;
    supplementaryText.clear();
  }
  m_ui.messageLabel->setText(wrapLongRuns(request.message.empty() ? request.actionId : request.message));
  m_ui.promptLabel->setText(promptText);
  m_ui.promptLabel->setColor(
      isInvalidPassword ? colorSpecFromRole(ColorRole::Error) : colorSpecFromRole(ColorRole::OnSurface)
  );
  m_ui.promptLabel->setVisible(!promptText.empty());
  m_ui.supplementaryLabel->setText(supplementaryText);
  m_ui.supplementaryLabel->setVisible(!supplementaryText.empty());
  m_ui.supplementaryLabel->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
  m_ui.input->setVisible(needsInput);
  m_ui.submitButton->setVisible(needsInput);
  m_ui.submitButton->setEnabled(needsInput && !m_ui.input->value().empty());
  if (needsInput != m_lastResponseRequired) {
    if (auto* manager = PanelManager::current(); manager != nullptr && manager->isOpenPanel("polkit")) {
      manager->relayoutActivePanelPreferredSize();
      if (needsInput) {
        manager->focusArea(m_ui.input->inputArea());
      }
    }
  }
  m_lastResponseRequired = needsInput;

  if (request.iconName != m_lastIconName || !m_iconResolved) {
    m_lastIconName = request.iconName;
    m_iconResolved = true;
    resolveIcon(renderer, request);
  }
}

void PolkitPanel::resolveIcon(Renderer& renderer, const PolkitRequest& request) {
  const float iconSize = scaled(48.0F);
  if (m_ui.iconContainer != nullptr) {
    m_ui.iconContainer->setSize(iconSize, iconSize);
  }

  if (request.isInternal) {
    const auto logoPath = paths::assetPath("paw2.svg");
    m_ui.fallbackIcon->setVisible(false);
    m_ui.icon->setSize(iconSize, iconSize);
    m_ui.icon->setSourceFile(renderer, logoPath.string(), static_cast<int>(std::round(iconSize)), true);
    m_ui.icon->setVisible(true);
    return;
  }

  if (!request.iconName.empty()) {
    const std::string& resolved = m_iconResolver.resolve(request.iconName, static_cast<int>(std::round(iconSize)));
    if (!resolved.empty()) {
      m_ui.fallbackIcon->setVisible(false);
      m_ui.icon->setSize(iconSize, iconSize);
      m_ui.icon->setSourceFile(renderer, resolved, static_cast<int>(std::round(iconSize)), true);
      m_ui.icon->setVisible(true);
      return;
    }
  }

  m_ui.icon->clear(renderer);
  m_ui.icon->setVisible(false);
  m_ui.fallbackIcon->setVisible(true);
}

void PolkitPanel::submit(std::string_view response) {
  PolkitAgent* agent = m_agentProvider != nullptr ? m_agentProvider() : nullptr;
  if (agent == nullptr || m_ui.input == nullptr) {
    return;
  }
  const std::string password = response.empty() ? m_ui.input->value() : std::string(response);
  if (password.empty()) {
    return;
  }
  agent->submitResponse(password);
  m_ui.input->setValue("");
}

bool PolkitPanel::handleInputKeyEvent(std::uint32_t sym, std::uint32_t modifiers) {
  if (KeybindMatcher::matches(KeybindAction::Validate, sym, modifiers)) {
    submit();
    return true;
  }
  const bool shift = (modifiers & KeyMod::Shift) != 0;
  if (KeybindMatcher::matches(KeybindAction::Left, sym, modifiers)) {
    if (m_ui.input != nullptr) {
      m_ui.input->moveCaretLeft(shift);
    }
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::Right, sym, modifiers)) {
    if (m_ui.input != nullptr) {
      m_ui.input->moveCaretRight(shift);
    }
    return true;
  }
  return false;
}

void PolkitPanel::onPanelCardOpacityChanged(float opacity) {
  if (m_ui.input != nullptr) {
    m_ui.input->setSurfaceOpacity(opacity);
  }
}
