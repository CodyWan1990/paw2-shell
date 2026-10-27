#include "shell/themes/themes_panel.h"

#include "config/config_service.h"
#include "core/input/keybind_matcher.h"
#include "i18n/i18n.h"
#include "render/core/async_texture_cache.h"
#include "render/core/renderer.h"
#include "shell/panel/panel_manager.h"
#include "shell/panel/panel_scaffold.h"
#include "shell/themes/theme_palette_choices.h"
#include "theme/theme_presets.h"
#include "ui/builders.h"
#include "ui/controls/color_swatch_preview.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace {

  constexpr float kCardWidth = 260.0F;
  constexpr float kCardImageHeight = 146.0F;

  // A text line's height, as the other panels estimate it.
  constexpr float lineHeight(float fontSize) { return fontSize * 1.35F; }

  std::string fileNameOf(std::string_view path) { return std::filesystem::path(path).filename().string(); }

} // namespace

ThemesPanel::ThemesPanel(paw2shell::theme::ThemePresetService* presets, ConfigService* config, AsyncTextureCache* textures)
    : m_presets(presets), m_config(config), m_textures(textures) {
  if (m_presets != nullptr) {
    m_presetsChanged = m_presets->addChangeCallback([this]() {
      m_dirty = true;
      if (m_scroll != nullptr) {
        PanelManager::instance().refresh();
      }
    });
  }
}

void ThemesPanel::create() {
  const float scale = contentScale();
  auto root = panel_scaffold::rootColumn(&m_rootLayout, scale);

  root->addChild(
      panel_scaffold::header(
          "palette", i18n::tr("themes.panel.title"), scale,
          ui::button({
              .out = &m_randomButton,
              .text = i18n::tr("themes.panel.random"),
              .glyph = "arrows-shuffle",
              .variant = ButtonVariant::Outline,
              .visible = false,
              .onClick =
                  [this]() {
                    if (m_presets != nullptr) {
                      (void)m_presets->randomWallpaper();
                    }
                  },
          }),
          ui::segmented({
              .out = &m_viewSwitch,
              .options =
                  std::vector<ui::SegmentedOption>{
                      {.label = i18n::tr("themes.panel.view.themes"), .glyph = "palette", .tooltip = {}},
                      {.label = i18n::tr("themes.panel.view.wallpapers"), .glyph = "photo", .tooltip = {}},
                  },
              .selectedIndex = static_cast<std::size_t>(m_view),
              .onChange = [this](std::size_t index) { setView(index == 0 ? View::Themes : View::Wallpapers); },
          })
      )
  );

  root->addChild(
      ui::scrollView({
          .out = &m_scroll,
          .contentScale = scale,
          .orientation = ScrollOrientation::Horizontal,
          .fillWidth = true,
          .flexGrow = 1.0F,
          .configure = [](ScrollView& view) { view.setVerticalWheelScrollsAcross(true); },
      })
  );

  root->addChild(panel_scaffold::hint(i18n::tr("themes.panel.hint"), scale));

  setRoot(std::move(root));
}

void ThemesPanel::onOpen(std::string_view context) {
  m_view = context == "wallpapers" ? View::Wallpapers : View::Themes;
  if (m_viewSwitch != nullptr) {
    m_viewSwitch->setSelectedIndex(static_cast<std::size_t>(m_view));
  }
  m_selected = 0;
  rebuildCards();
}

void ThemesPanel::onClose() {
  clearReleasedRoot();
  m_rootLayout = nullptr;
  m_scroll = nullptr;
  m_viewSwitch = nullptr;
  m_randomButton = nullptr;
  m_cards.clear();
  m_images.clear();
}

void ThemesPanel::setView(View view) {
  m_view = view;
  m_selected = 0;
  rebuildCards();
  PanelManager::instance().refresh();
}

// Tall enough for the tallest card: a theme card with its swatches and the Edit button under it.
// A fixed height once cut the Edit row off below the scrollbar.
float ThemesPanel::preferredHeight() const {
  const float card = Style::spaceSm * 2.0F // card padding
      + kCardImageHeight
      + Style::spaceSm
      + lineHeight(Style::fontSizeBody) // title
      + Style::spaceXs
      + lineHeight(Style::fontSizeCaption);                              // swatches
  const float cardColumn = card + Style::spaceXs + Style::controlHeight; // the Edit button
  const float strip = cardColumn + Style::scrollbarGap + Style::scrollbarHoverWidth;
  const float root = Style::spaceLg * 2.0F // rootColumn padding
      + Style::controlHeight               // header
      + strip
      + lineHeight(Style::fontSizeCaption) // hint
      + Style::spaceMd * 2.0F;             // gaps between the three
  return std::ceil(scaled(root + Style::panelPadding * 2.0F));
}

void ThemesPanel::openEditor(std::string_view id) {
  PanelManager::instance().openPanel("theme-editor", PanelOpenRequest{.context = id.empty() ? "new" : id});
}

std::unique_ptr<Node> ThemesPanel::makeCard(
    std::string_view imagePath, bool current, bool selected, std::unique_ptr<Node> caption,
    std::function<void()> onClick, std::function<void()> onEdit
) {
  const float scale = contentScale();
  Image* image = nullptr;
  auto card = ui::button({
      .variant = current ? ButtonVariant::Outline : ButtonVariant::Ghost,
      .surfaceOpacity = panelCardOpacity(),
      .padding = Style::spaceSm * scale,
      .gap = Style::spaceSm * scale,
      .radius = Style::scaledRadiusLg(scale),
      .width = kCardWidth * scale,
      .onClick = std::move(onClick),
      .onRightClick = onEdit,
      .configure =
          [](Button& control) {
            control.setDirection(FlexDirection::Vertical);
            control.setAlign(FlexAlign::Stretch);
          },
  });
  card->setHoveredVisual(selected);
  card->addChild(ui::image({
      .out = &image,
      .fit = ImageFit::Cover,
      .radius = Style::radiusMd * scale,
      .height = kCardImageHeight * scale,
  }));
  if (!imagePath.empty()) {
    image->setAsyncReadyCallback([]() { PanelManager::instance().refresh(); });
    m_images.push_back({.image = image, .path = std::string(imagePath)});
  }
  card->addChild(std::move(caption));
  m_cards.push_back(card.get());
  if (!onEdit) {
    return card;
  }
  // The edit button sits under the card rather than in it: the card is a button
  // itself, and would take the click.
  return ui::column(
      {.align = FlexAlign::Stretch, .gap = Style::spaceXs * scale},
      std::move(card),
      ui::button({
          .text = i18n::tr("themes.panel.edit"),
          .glyph = "pencil",
          .variant = ButtonVariant::Ghost,
          .onClick = std::move(onEdit),
      })
  );
}

void ThemesPanel::rebuildCards() {
  m_dirty = false;
  if (m_scroll == nullptr) {
    return;
  }
  Flex* strip = m_scroll->content();
  strip->clearChildren();
  strip->setDirection(FlexDirection::Horizontal);
  strip->setGap(Style::spaceMd * contentScale());
  m_cards.clear();
  m_images.clear();
  if (m_randomButton != nullptr) {
    m_randomButton->setVisible(m_view == View::Wallpapers);
  }
  if (m_view == View::Themes) {
    addThemeCards();
  } else {
    addWallpaperCards();
  }
  if (!m_cards.empty()) {
    m_selected = std::min(m_selected, m_cards.size() - 1);
    m_scroll->requestScrollIntoView(m_cards[m_selected]);
  }
}

void ThemesPanel::addThemeCards() {
  const float scale = contentScale();
  Flex* strip = m_scroll->content();
  const ThemeMode mode = m_config != nullptr ? m_config->config().theme.mode : ThemeMode::Dark;

  for (const auto& preset : m_presets->presets()) {
    const std::size_t index = m_cards.size();
    auto caption = ui::column({.align = FlexAlign::Stretch, .gap = Style::spaceXs * scale});
    caption->addChild(ui::label({
        .text = preset.title,
        .fontSize = Style::fontSizeBody * scale,
        .fontWeight = FontWeight::Bold,
        .color = colorSpecFromRole(ColorRole::OnSurface),
        .maxLines = 1,
        .ellipsize = TextEllipsize::End,
    }));
    if (const auto preview = themes::presetPreview(preset, mode); !preview.empty()) {
      auto swatches = std::make_unique<ColorSwatchPreviewStrip>();
      swatches->setMetricsFromFontSize(Style::fontSizeCaption * scale);
      swatches->setPreview(preview);
      caption->addChild(std::move(swatches));
    } else {
      caption->addChild(ui::label({
          .text = i18n::tr("themes.panel.from-wallpaper"),
          .fontSize = Style::fontSizeCaption * scale,
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
      }));
    }
    const std::string picture = !preset.screenshotPath.empty() ? preset.screenshotPath : preset.currentWallpaper();
    strip->addChild(makeCard(
        picture, preset.id == m_presets->lastAppliedId(), index == m_selected, std::move(caption),
        [this, index]() { activate(index); }, [id = preset.id]() { openEditor(id); }
    ));
  }

  // The last card makes a new theme.
  const std::size_t index = m_cards.size();
  auto caption = ui::row(
      {.align = FlexAlign::Center, .justify = FlexJustify::Center, .gap = Style::spaceXs * scale, .fillWidth = true},
      ui::glyph({.glyph = "plus", .glyphSize = Style::fontSizeBody * scale, .color = colorSpecFromRole(ColorRole::Primary)}),
      ui::label({
          .text = i18n::tr("themes.panel.new"),
          .fontSize = Style::fontSizeBody * scale,
          .color = colorSpecFromRole(ColorRole::OnSurface),
      })
  );
  strip->addChild(makeCard({}, false, index == m_selected, std::move(caption), [this, index]() { activate(index); }));
}

void ThemesPanel::addWallpaperCards() {
  const float scale = contentScale();
  Flex* strip = m_scroll->content();
  const auto* applied = m_presets->applied();
  if (applied == nullptr || applied->wallpapers.empty()) {
    strip->addChild(ui::label({
        .text = i18n::tr(applied == nullptr ? "themes.panel.none-applied" : "themes.panel.no-wallpapers"),
        .fontSize = Style::fontSizeBody * scale,
        .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
    }));
    return;
  }
  for (std::size_t i = 0; i < applied->wallpapers.size(); ++i) {
    const std::size_t index = m_cards.size();
    auto caption = ui::label({
        .text = fileNameOf(applied->wallpapers[i]),
        .fontSize = Style::fontSizeCaption * scale,
        .color = colorSpecFromRole(ColorRole::OnSurface),
        .maxLines = 1,
        .ellipsize = TextEllipsize::Middle,
    });
    strip->addChild(makeCard(
        applied->wallpapers[i], i == applied->wallpaperIndex, index == m_selected, std::move(caption),
        [this, index]() { activate(index); }
    ));
  }
}

void ThemesPanel::activate(std::size_t index) {
  if (m_presets == nullptr) {
    return;
  }
  m_selected = index;
  if (m_view == View::Wallpapers) {
    (void)m_presets->showWallpaper(index);
    return;
  }
  const auto& presets = m_presets->presets();
  if (index >= presets.size()) {
    openEditor({});
    return;
  }
  (void)m_presets->apply(presets[index].id);
}

void ThemesPanel::select(std::size_t index) {
  if (m_cards.empty()) {
    return;
  }
  m_selected = std::min(index, m_cards.size() - 1);
  for (std::size_t i = 0; i < m_cards.size(); ++i) {
    m_cards[i]->setHoveredVisual(i == m_selected);
  }
  if (m_scroll != nullptr) {
    m_scroll->requestScrollIntoView(m_cards[m_selected]);
  }
  PanelManager::instance().refresh();
}

bool ThemesPanel::handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool /*preedit*/) {
  if (!pressed) {
    return false;
  }
  if (KeybindMatcher::matches(KeybindAction::Left, sym, modifiers)) {
    select(m_selected == 0 ? 0 : m_selected - 1);
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::Right, sym, modifiers)) {
    select(m_selected + 1);
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::Validate, sym, modifiers)) {
    activate(m_selected);
    return true;
  }
  if (KeybindMatcher::matches(KeybindAction::TabNext, sym, modifiers)) {
    setView(m_view == View::Themes ? View::Wallpapers : View::Themes);
    if (m_viewSwitch != nullptr) {
      m_viewSwitch->setSelectedIndex(static_cast<std::size_t>(m_view));
    }
    return true;
  }
  return false;
}

void ThemesPanel::onPanelCardOpacityChanged(float opacity) {
  for (Button* card : m_cards) {
    card->setSurfaceOpacity(opacity);
  }
}

void ThemesPanel::doUpdate(Renderer& renderer) {
  if (m_dirty) {
    rebuildCards();
  }
  if (m_textures == nullptr) {
    return;
  }
  const int targetSize = static_cast<int>(std::ceil(kCardWidth * contentScale()));
  for (const auto& card : m_images) {
    (void)card.image->setSourceFileAsync(renderer, *m_textures, card.path, targetSize, true);
  }
}

void ThemesPanel::doLayout(Renderer& renderer, float width, float height) {
  if (m_rootLayout == nullptr) {
    return;
  }
  doUpdate(renderer);
  m_rootLayout->setSize(width, height);
  m_rootLayout->layout(renderer);
  for (Button* card : m_cards) {
    card->updateInputArea();
  }
}
