#include "shell/themes/theme_editor_panel.h"

#include "capture/screenshot_service.h"
#include "config/config_service.h"
#include "core/deferred_call.h"
#include "core/input/keybind_matcher.h"
#include "i18n/i18n.h"
#include "render/core/async_texture_cache.h"
#include "render/core/renderer.h"
#include "shell/panel/panel_manager.h"
#include "shell/wallpaper/panel/wallpaper_panel.h"
#include "ui/builders.h"
#include "ui/palette.h"
#include "ui/style.h"
#include "util/file_utils.h"
#include "util/string_utils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace {

  constexpr float kThumbWidth = 72.0F;
  constexpr float kThumbHeight = 40.0F;
  constexpr float kPreviewHeight = 150.0F;

  // Segment order of the source switch; matches PaletteSource's values.
  constexpr std::array kSources{
      PaletteSource::Builtin, PaletteSource::Wallpaper, PaletteSource::Community, PaletteSource::Custom
  };

  std::unique_ptr<Label> sectionLabel(std::string_view key, float scale) {
    return ui::label({
        .text = i18n::tr(key),
        .fontSize = Style::fontSizeBody * scale,
        .fontWeight = FontWeight::Bold,
        .color = colorSpecFromRole(ColorRole::OnSurface),
    });
  }

  // Each capture gets its own file: images are cached by path, so overwriting
  // one would keep showing the old picture.
  std::filesystem::path newPreviewPath(std::string_view id) {
    const auto stamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
    return std::filesystem::path(FileUtils::stateDir()) / "theme-previews"
         / (std::string(id) + "-" + std::to_string(stamp) + ".png");
  }

} // namespace

ThemeEditorPanel::ThemeEditorPanel(
    noctalia::theme::ThemePresetService* presets, ConfigService* config, ScreenshotService* screenshots,
    std::function<wl_output*()> captureOutput, AsyncTextureCache* textures
)
    : m_presets(presets), m_config(config), m_screenshots(screenshots), m_captureOutput(std::move(captureOutput)),
      m_textures(textures) {}

void ThemeEditorPanel::create() {
  const float scale = contentScale();
  auto root = ui::column({
      .out = &m_ui.rootLayout,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
      .padding = Style::spaceLg * scale,
  });

  root->addChild(
      ui::label({
          .out = &m_ui.heading,
          .fontSize = Style::fontSizeTitle * scale,
          .fontWeight = FontWeight::Bold,
          .color = colorSpecFromRole(ColorRole::OnSurface),
      })
  );

  root->addChild(sectionLabel("themes.editor.name", scale));
  root->addChild(
      ui::input({
          .out = &m_ui.titleInput,
          .placeholder = i18n::tr("themes.editor.name-placeholder"),
          .surfaceOpacity = panelCardOpacity(),
          .onChange = [this](const std::string& value) {
            m_draft.title = value;
            syncButtons();
          },
      })
  );

  root->addChild(sectionLabel("themes.editor.palette", scale));
  root->addChild(
      ui::segmented({
          .out = &m_ui.sourceSwitch,
          .options =
              std::vector<ui::SegmentedOption>{
                  {.label = i18n::tr("settings.options.theme.source.built-in"), .glyph = {}, .tooltip = {}},
                  {.label = i18n::tr("settings.options.theme.source.wallpaper"), .glyph = {}, .tooltip = {}},
                  {.label = i18n::tr("settings.options.theme.source.community"), .glyph = {}, .tooltip = {}},
                  {.label = i18n::tr("settings.options.theme.source.custom"), .glyph = {}, .tooltip = {}},
              },
          .equalSegmentWidths = true,
          .onChange = [this](std::size_t index) {
            m_draft.source = kSources[std::min(index, kSources.size() - 1)];
            syncPaletteSelect();
            syncButtons();
          },
      })
  );
  root->addChild(
      ui::select({
          .out = &m_ui.paletteSelect,
          .placeholder = i18n::tr("themes.editor.palette-placeholder"),
          .surfaceOpacity = panelCardOpacity(),
          .onSelectionChanged = [this](std::size_t index, std::string_view /*text*/) {
            if (index >= m_choices.size()) {
              return;
            }
            if (m_draft.source == PaletteSource::Wallpaper) {
              m_draft.wallpaperScheme = m_choices[index].value;
            } else {
              m_draft.palette = m_choices[index].value;
            }
            syncButtons();
          },
      })
  );

  root->addChild(ui::row(
      {.align = FlexAlign::Center, .gap = Style::spaceSm * scale, .fillWidth = true},
      sectionLabel("themes.editor.wallpapers", scale),
      ui::spacer(),
      ui::button({
          .text = i18n::tr("themes.editor.add-current"),
          .glyph = "photo",
          .variant = ButtonVariant::Outline,
          .onClick = [this]() { addCurrentWallpaper(); },
      }),
      ui::button({
          .text = i18n::tr("themes.editor.browse"),
          .glyph = "folder-open",
          .variant = ButtonVariant::Outline,
          .onClick = [this]() { browseWallpapers(); },
      })
  ));
  auto wallpapers = ui::scrollView({
      .contentScale = scale,
      .fillWidth = true,
      .flexGrow = 1.0F,
  });
  m_ui.wallpaperList = wallpapers->content();
  root->addChild(std::move(wallpapers));

  root->addChild(ui::row(
      {.align = FlexAlign::Center, .gap = Style::spaceSm * scale, .fillWidth = true},
      sectionLabel("themes.editor.preview", scale),
      ui::spacer(),
      ui::button({
          .text = i18n::tr("themes.editor.use-wallpaper"),
          .variant = ButtonVariant::Ghost,
          .onClick =
              [this]() {
                m_draft.screenshotPath.clear();
                syncPreview();
              },
      }),
      ui::button({
          .text = i18n::tr("themes.editor.capture"),
          .glyph = "camera",
          .variant = ButtonVariant::Outline,
          .onClick = [this]() { captureDesktop(); },
      })
  ));
  root->addChild(
      ui::image({
          .out = &m_ui.preview,
          .fit = ImageFit::Cover,
          .radius = Style::radiusMd * scale,
          .height = kPreviewHeight * scale,
      })
  );
  m_ui.preview->setAsyncReadyCallback([]() { PanelManager::instance().refresh(); });
  root->addChild(
      ui::label({
          .out = &m_ui.previewNote,
          .fontSize = Style::fontSizeCaption * scale,
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
      })
  );

  root->addChild(
      ui::row(
          {.align = FlexAlign::Center, .gap = Style::spaceSm * scale, .fillWidth = true},
          ui::button({
              .out = &m_ui.deleteButton,
              .text = i18n::tr("themes.editor.delete"),
              .glyph = "trash",
              .variant = ButtonVariant::Destructive,
              .onClick = [this]() { requestDelete(); },
          }),
          ui::spacer(),
          ui::button({
              .text = i18n::tr("common.actions.cancel"),
              .variant = ButtonVariant::Outline,
              .onClick = []() { backToThemes(); },
          }),
          ui::button({
              .out = &m_ui.saveButton,
              .text = i18n::tr("themes.editor.save"),
              .glyph = "check",
              .variant = ButtonVariant::Primary,
              .onClick = [this]() { save(); },
          })
      )
  );

  setRoot(std::move(root));
}

void ThemeEditorPanel::onOpen(std::string_view context) {
  if (context != "resume") {
    const auto* existing = m_presets != nullptr ? m_presets->byId(context) : nullptr;
    m_editingExisting = existing != nullptr;
    m_draft = existing != nullptr ? *existing : noctalia::theme::ThemePreset{};
    if (existing == nullptr && m_config != nullptr) {
      // A new theme starts from the look on screen now.
      const auto& theme = m_config->config().theme;
      m_draft.source = theme.source;
      m_draft.palette = theme.source == PaletteSource::Community ? theme.communityPalette
                      : theme.source == PaletteSource::Custom    ? theme.customPalette
                                                                 : theme.builtinPalette;
      m_draft.wallpaperScheme = theme.wallpaperScheme;
    }
  }
  m_confirmingDelete = false;
  syncFromDraft();
}

void ThemeEditorPanel::onClose() {
  clearReleasedRoot();
  m_ui = {};
  m_thumbs.clear();
  m_previewPath.clear();
}

InputArea* ThemeEditorPanel::initialFocusArea() const {
  return m_ui.titleInput != nullptr ? m_ui.titleInput->inputArea() : nullptr;
}

void ThemeEditorPanel::syncFromDraft() {
  if (m_ui.heading != nullptr) {
    m_ui.heading->setText(i18n::tr(m_editingExisting ? "themes.editor.edit-title" : "themes.editor.new-title"));
  }
  if (m_ui.titleInput != nullptr) {
    m_ui.titleInput->setValue(m_draft.title);
  }
  if (m_ui.sourceSwitch != nullptr) {
    m_ui.sourceSwitch->setSelectedIndex(static_cast<std::size_t>(m_draft.source));
  }
  syncPaletteSelect();
  rebuildWallpaperList();
  syncPreview();
  syncButtons();
}

void ThemeEditorPanel::syncPaletteSelect() {
  if (m_ui.paletteSelect == nullptr) {
    return;
  }
  const ThemeMode mode = m_config != nullptr ? m_config->config().theme.mode : ThemeMode::Dark;
  m_choices = themes::paletteChoices(m_draft.source, mode);
  std::vector<std::string> labels;
  std::vector<ColorSwatchPreview> previews;
  for (const auto& choice : m_choices) {
    labels.push_back(choice.label);
    previews.push_back(choice.preview);
  }
  m_ui.paletteSelect->setOptions(std::move(labels));
  m_ui.paletteSelect->setColorSwatchPreviews(std::move(previews));
  const auto it = std::ranges::find(m_choices, m_draft.paletteValue(), &themes::PaletteChoice::value);
  if (it != m_choices.end()) {
    m_ui.paletteSelect->setSelectedIndexSilently(static_cast<std::size_t>(it - m_choices.begin()));
  } else if (m_draft.source == PaletteSource::Wallpaper && !m_choices.empty()) {
    // The wallpaper source always has a scheme; start from the first.
    m_draft.wallpaperScheme = m_choices.front().value;
    m_ui.paletteSelect->setSelectedIndexSilently(0);
  }
}

void ThemeEditorPanel::rebuildWallpaperList() {
  if (m_ui.wallpaperList == nullptr) {
    return;
  }
  const float scale = contentScale();
  m_ui.wallpaperList->clearChildren();
  m_thumbs.clear();
  m_ui.wallpaperList->setGap(Style::spaceXs * scale);

  if (m_draft.wallpapers.empty()) {
    m_ui.wallpaperList->addChild(
        ui::label({
            .text = i18n::tr("themes.editor.no-wallpapers"),
            .fontSize = Style::fontSizeCaption * scale,
            .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
        })
    );
    return;
  }
  for (std::size_t i = 0; i < m_draft.wallpapers.size(); ++i) {
    const std::string& path = m_draft.wallpapers[i];
    Image* thumb = nullptr;
    m_ui.wallpaperList->addChild(
        ui::row(
            {.align = FlexAlign::Center, .gap = Style::spaceSm * scale, .fillWidth = true},
            ui::image({
                .out = &thumb,
                .fit = ImageFit::Cover,
                .radius = Style::radiusSm * scale,
                .width = kThumbWidth * scale,
                .height = kThumbHeight * scale,
            }),
            ui::label({
                .text = std::filesystem::path(path).filename().string(),
                .fontSize = Style::fontSizeBody * scale,
                .color = colorSpecFromRole(ColorRole::OnSurface),
                .maxLines = 1,
                .ellipsize = TextEllipsize::Middle,
                .flexGrow = 1.0F,
            }),
            ui::button({
                .glyph = "x",
                .variant = ButtonVariant::Ghost,
                .tooltip = i18n::tr("themes.editor.remove-wallpaper"),
                .onClick = [this, i]() {
                  if (i < m_draft.wallpapers.size()) {
                    m_draft.wallpapers.erase(m_draft.wallpapers.begin() + static_cast<long>(i));
                    m_draft.wallpaperIndex = 0;
                    rebuildWallpaperList();
                    syncPreview();
                    PanelManager::instance().refresh();
                  }
                },
            })
        )
    );
    thumb->setAsyncReadyCallback([]() { PanelManager::instance().refresh(); });
    m_thumbs.push_back({.image = thumb, .path = path});
  }
}

void ThemeEditorPanel::syncPreview() {
  // A captured screenshot when there is one, else the theme's wallpaper.
  m_previewPath = !m_draft.screenshotPath.empty() ? m_draft.screenshotPath : m_draft.currentWallpaper();
  if (m_ui.previewNote != nullptr) {
    m_ui.previewNote->setText(
        i18n::tr(
            !m_draft.screenshotPath.empty() ? "themes.editor.preview-captured"
                : m_previewPath.empty()     ? "themes.editor.preview-none"
                                            : "themes.editor.preview-wallpaper"
        )
    );
  }
}

void ThemeEditorPanel::syncButtons() {
  if (m_ui.saveButton != nullptr) {
    m_ui.saveButton->setEnabled(!StringUtils::trim(m_draft.title).empty() && !m_draft.paletteValue().empty());
  }
  if (m_ui.deleteButton != nullptr) {
    m_ui.deleteButton->setVisible(m_editingExisting);
    m_ui.deleteButton->setText(i18n::tr(m_confirmingDelete ? "themes.editor.delete-confirm" : "themes.editor.delete"));
  }
}

void ThemeEditorPanel::addCurrentWallpaper() {
  if (m_config == nullptr) {
    return;
  }
  const std::string path = m_config->getPaletteWallpaperPath();
  if (path.empty() || std::ranges::contains(m_draft.wallpapers, path)) {
    return;
  }
  m_draft.wallpapers.push_back(path);
  rebuildWallpaperList();
  syncPreview();
  PanelManager::instance().refresh();
}

void ThemeEditorPanel::browseWallpapers() {
  WallpaperPanel::requestPick([this](std::vector<std::string> paths) {
    for (auto& path : paths) {
      if (!std::ranges::contains(m_draft.wallpapers, path)) {
        m_draft.wallpapers.push_back(std::move(path));
      }
    }
    resumeEditing();
  });
  PanelManager::instance().openPanel("wallpaper", PanelOpenRequest{.context = "pick"});
}

void ThemeEditorPanel::captureDesktop() {
  if (m_screenshots == nullptr || !m_captureOutput) {
    return;
  }
  const auto path = newPreviewPath(m_draft.id.empty() ? m_presets->uniqueSlug(m_draft.title) : m_draft.id);
  // The editor must be off screen, or the preview would be a picture of itself.
  PanelManager::instance().closePanelForCapture([this, path]() {
    m_screenshots->captureToFile(m_captureOutput(), path, [this, path](bool written) {
      if (written) {
        m_draft.screenshotPath = path.string();
      }
      resumeEditing();
    });
  });
}

void ThemeEditorPanel::save() {
  if (m_presets == nullptr || StringUtils::trim(m_draft.title).empty() || m_draft.paletteValue().empty()) {
    return;
  }
  m_draft.title = StringUtils::trim(m_draft.title);
  m_draft.id = m_presets->save(m_draft);
  backToThemes();
}

void ThemeEditorPanel::requestDelete() {
  if (!m_editingExisting || m_presets == nullptr) {
    return;
  }
  // The first press arms the button, the second deletes.
  if (!m_confirmingDelete) {
    m_confirmingDelete = true;
    syncButtons();
    PanelManager::instance().refresh();
    return;
  }
  (void)m_presets->remove(m_draft.id);
  backToThemes();
}

void ThemeEditorPanel::backToThemes() { PanelManager::instance().openPanel("themes"); }

void ThemeEditorPanel::resumeEditing() {
  DeferredCall::callLater([]() { PanelManager::instance().openPanel("theme-editor", PanelOpenRequest{.context = "resume"}); });
}

bool ThemeEditorPanel::handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool /*preedit*/) {
  if (!pressed) {
    return false;
  }
  if (KeybindMatcher::matches(KeybindAction::Cancel, sym, modifiers)) {
    backToThemes();
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::Save, sym, modifiers)) {
    save();
    return true;
  }
  return false;
}

void ThemeEditorPanel::doUpdate(Renderer& renderer) {
  if (m_textures == nullptr) {
    return;
  }
  const float scale = contentScale();
  for (const auto& thumb : m_thumbs) {
    (void)thumb.image->setSourceFileAsync(
        renderer, *m_textures, thumb.path, static_cast<int>(std::ceil(kThumbWidth * scale)), true
    );
  }
  if (m_ui.preview != nullptr) {
    if (m_previewPath.empty()) {
      m_ui.preview->clear(renderer);
    } else {
      (void)m_ui.preview->setSourceFileAsync(
          renderer, *m_textures, m_previewPath, static_cast<int>(std::ceil(preferredWidth())), true
      );
    }
  }
}

void ThemeEditorPanel::doLayout(Renderer& renderer, float width, float height) {
  if (m_ui.rootLayout == nullptr) {
    return;
  }
  doUpdate(renderer);
  m_ui.rootLayout->setSize(width, height);
  m_ui.rootLayout->layout(renderer);
}
