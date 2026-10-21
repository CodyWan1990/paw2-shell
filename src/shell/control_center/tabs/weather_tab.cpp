#include "shell/control_center/tabs/weather_tab.h"

#include "config/config_service.h"
#include "i18n/i18n.h"
#include "render/animation/animation.h"
#include "render/animation/animation_manager.h"
#include "render/scene/effect_node.h"
#include "render/scene/input_area.h"
#include "shell/panel/panel_manager.h"
#include "shell/tooltip/tooltip_content.h"
#include "system/weather_service.h"
#include "time/time_format.h"
#include "ui/builders.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <memory>

using namespace control_center;

namespace {

  // Set to a specific effect to bypass weather-code detection. Reset to None when done testing.
  constexpr EffectType kTestEffect = EffectType::None;

  constexpr float kCurrentGlyphSize = Style::controlHeightLg * 2.2F;

  std::string windDirectionLabel(int degrees) {
    static constexpr std::array<const char*, 8> kDirs = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    const int normalized = ((degrees % 360) + 360) % 360;
    const int index = static_cast<int>(std::lround(normalized / 45.0)) % 8;
    return kDirs[static_cast<std::size_t>(index)];
  }

} // namespace

WeatherTab::WeatherTab(WeatherService* weather, ConfigService* config) : m_weather(weather), m_config(config) {
  m_forecastRows.fill(nullptr);
  m_forecastSeparators.fill(nullptr);
  m_forecastIconSlots.fill(nullptr);
  m_forecastGlyphs.fill(nullptr);
  m_forecastMetas.fill(nullptr);
  m_forecastDescs.fill(nullptr);
  m_forecastTemps.fill(nullptr);
  m_forecastHitAreas.fill(nullptr);
}

std::unique_ptr<Flex> WeatherTab::create() {
  const float scale = contentScale();
  auto tab = ui::row({
      .out = &m_ui.rootLayout,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
  });

  auto leftColumn = ui::column({
      .out = &m_ui.leftColumn,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceMd * scale,
      .flexGrow = 3.0F,
  });

  auto currentCard = ui::row({
      .out = &m_ui.currentCard,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceSm * scale,
      .padding = Style::spaceXs * scale,
      .clipChildren = true,
      .flexGrow = 1.0F,
      .cardStyleScale = scale,
      .cardStyleFillOpacity = panelCardOpacity(),
      .configure = [scale](Flex& card) {
        card.setDirection(FlexDirection::Horizontal);
        card.setPadding(Style::spaceXs * scale, Style::spaceXs * scale);
        card.setGap(Style::spaceSm * scale);
      },
  });

  auto effectNode = std::make_unique<EffectNode>();
  effectNode->setParticipatesInLayout(false);
  effectNode->setZIndex(-1);
  effectNode->setVisible(false);
  effectNode->setRadius(Style::scaledRadiusXl(scale));
  m_ui.effectNode = static_cast<EffectNode*>(currentCard->addChild(std::move(effectNode)));

  auto glyphColumn = ui::row(
      {.out = &m_ui.glyphColumn,
       .align = FlexAlign::Center,
       .justify = FlexJustify::End,
       .fillHeight = true,
       .flexGrow = 0.9F},
      ui::glyph({
          .out = &m_ui.currentGlyph,
          .glyph = "weather-cloud",
          .glyphSize = kCurrentGlyphSize * scale,
          .color = colorSpecFromRole(ColorRole::Primary),
      })
  );
  currentCard->addChild(std::move(glyphColumn));

  auto currentText = ui::column(
      {.out = &m_ui.currentText,
       .align = FlexAlign::Stretch,
       .justify = FlexJustify::Center,
       .gap = Style::spaceXs * scale,
       .fillWidth = true,
       .flexGrow = 1.0F}
  );

  currentText->addChild(
      ui::column(
          {.align = FlexAlign::Stretch, .gap = Style::spaceXs * scale},
          ui::label({
              .out = &m_ui.currentTempLabel,
              .text = "--°C",
              .fontSize = Style::fontSizeTitle * 2.35F * scale,
              .fontWeight = FontWeight::Bold,
              .color = colorSpecFromRole(ColorRole::OnSurface),
              .maxLines = 1,
          }),
          ui::label({
              .out = &m_ui.currentHiLoLabel,
              .text = "--↑ --↓",
              .fontSize = Style::fontSizeBody * scale,
              .color = colorSpecFromRole(ColorRole::Primary),
              .maxLines = 1,
          })
      )
  );

  currentText->addChild(
      ui::column(
          {.align = FlexAlign::Stretch, .gap = Style::spaceXs * 0.5F * scale},
          ui::label({
              .out = &m_ui.currentDescLabel,
              .text = i18n::tr("control-center.weather.waiting"),
              .fontSize = Style::fontSizeBody * scale,
              .color = colorSpecFromRole(ColorRole::OnSurface),
              .maxLines = 1,
          }),
          ui::label({
              .out = &m_ui.updatedLabel,
              .text = " ",
              .fontSize = Style::fontSizeCaption * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .maxLines = 1,
          }),
          ui::label({
              .out = &m_ui.statusLabel,
              .text = " ",
              .fontSize = Style::fontSizeCaption * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .maxLines = 1,
              .visible = false,
          })
      )
  );

  currentCard->addChild(std::move(currentText));

  auto locationPrompt = ui::row(
      {.out = &m_ui.locationPrompt,
       .align = FlexAlign::Center,
       .justify = FlexJustify::Center,
       .gap = Style::spaceMd * scale,
       .fillWidth = true,
       .fillHeight = true,
       .flexGrow = 1.0F,
       .visible = false},
      ui::glyph({
          .out = &m_ui.locationPromptGlyph,
          .glyph = "map-pin-off",
          .glyphSize = Style::controlHeightLg * scale,
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
      }),
      ui::column(
          {.align = FlexAlign::Stretch, .justify = FlexJustify::Center, .gap = Style::spaceXs * scale},
          ui::label({
              .text = i18n::tr("control-center.weather.no-location-title"),
              .fontSize = Style::fontSizeBody * 1.1F * scale,
              .fontWeight = FontWeight::Bold,
              .color = colorSpecFromRole(ColorRole::OnSurface),
              .maxLines = 1,
          }),
          ui::label({
              .out = &m_ui.locationPromptBody,
              .text = i18n::tr("control-center.weather.no-location-body"),
              .fontSize = Style::fontSizeBody * scale,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
              .maxLines = 2,
          })
      )
  );
  currentCard->addChild(std::move(locationPrompt));

  leftColumn->addChild(std::move(currentCard));

  auto detailsCard = ui::column({
      .out = &m_ui.detailsCard,
      .align = FlexAlign::Stretch,
      .gap = 0.0F,
      .cardStyleScale = scale,
      .cardStyleFillOpacity = panelCardOpacity(),
      .configure = [scale](Flex& card) {
        card.setPadding(Style::spaceMd * scale);
        card.setGap(0.0F);
      },
  });
  const float detailKeyWidth = Style::controlHeightLg * 2.0F * scale;
  // Detail rows are label/value pairs, not touch targets: they size to the text plus a small
  // margin around the row glyph, not to a control height.
  const float detailRowHeight = (Style::fontSizeBody + Style::spaceMd) * scale;

  auto addDetailRow = [&](std::string_view iconName, std::string_view key, Label*& valueOut) -> Flex* {
    auto row = ui::row({
        .align = FlexAlign::Center,
        .gap = (Style::spaceSm + Style::spaceXs) * scale,
        .minHeight = detailRowHeight,
        .flexGrow = 0.0F,
    });
    Flex* rowPtr = row.get();
    row->addChild(
        ui::glyph({
            .glyph = std::string(iconName),
            .glyphSize = (Style::fontSizeBody + Style::spaceXs) * scale,
            .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
        })
    );
    row->addChild(
        ui::label({
            .text = std::string(key),
            .fontSize = Style::fontSizeBody * scale,
            .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
            .minWidth = detailKeyWidth - (Style::fontSizeBody + Style::spaceXs) * scale - Style::spaceSm * scale,
        })
    );
    row->addChild(
        ui::label({
            .out = &valueOut,
            .text = "--",
            .fontSize = Style::fontSizeBody * scale,
            .fontWeight = FontWeight::Bold,
            .color = colorSpecFromRole(ColorRole::OnSurface),
            .textAlign = TextAlign::End,
            .flexGrow = 1.0F,
        })
    );
    detailsCard->addChild(std::move(row));
    return rowPtr;
  };

  addDetailRow("thermometer", i18n::tr("control-center.weather.details.feels-like"), m_ui.feelsLikeLabel);
  addDetailRow("temperature", i18n::tr("control-center.weather.details.temp-min"), m_ui.tempMinLabel);
  addDetailRow("temperature-sun", i18n::tr("control-center.weather.details.temp-max"), m_ui.tempMaxLabel);
  addDetailRow("wind", i18n::tr("control-center.weather.details.wind"), m_ui.windLabel);
  addDetailRow("weather-sunrise", i18n::tr("control-center.weather.details.sunrise"), m_ui.sunriseLabel);
  addDetailRow("weather-sunset", i18n::tr("control-center.weather.details.sunset"), m_ui.sunsetLabel);
  addDetailRow("mountain", i18n::tr("control-center.weather.details.elevation"), m_ui.elevationLabel);
  addDetailRow("sun", i18n::tr("control-center.weather.details.uv-index"), m_ui.uvIndexLabel);
  m_ui.timeZoneRow = addDetailRow("clock", i18n::tr("control-center.weather.details.timezone"), m_ui.timeZoneLabel);

  leftColumn->addChild(std::move(detailsCard));

  tab->addChild(std::move(leftColumn));

  auto forecastColumn = ui::column({
      .out = &m_ui.forecastColumn,
      .gap = Style::spaceXs * scale,
      .fillHeight = true,
      .flexGrow = 2.0F,
      .cardStyleScale = scale,
      .cardStyleFillOpacity = panelCardOpacity(),
      .configure = [scale](Flex& column) {
        column.setGap(Style::spaceXs * scale);
        column.setPadding(Style::spaceMd * scale, Style::spaceMd * scale);
        column.setClipChildren(true);
      },
  });

  forecastColumn->addChild(
      ui::segmented({
          .out = &m_ui.forecastViewPicker,
          .options =
              std::vector<ui::SegmentedOption>{
                  {.label = i18n::tr("control-center.weather.forecast-view.daily")},
                  {.label = i18n::tr("control-center.weather.forecast-view.hourly")},
              },
          .selectedIndex = static_cast<std::size_t>(m_forecastView),
          .fontSize = Style::fontSizeCaption,
          .scale = scale,
          .compact = true,
          .surfaceOpacity = panelCardOpacity(),
          .surfaceRole = ColorRole::Surface,
          .equalSegmentWidths = true,
          .onChange = [this](std::size_t idx) {
            const ForecastView nextView = idx == 1 ? ForecastView::Hourly : ForecastView::Daily;
            if (m_forecastView == nextView) {
              return;
            }
            beginForecastSlideOut(nextView);
          },
      })
  );

  auto forecastRowsContainer = ui::column({
      .out = &m_forecastRowsContainer,
      .align = FlexAlign::Stretch,
      .gap = Style::spaceXs * scale,
      .fillWidth = true,
      .flexGrow = 1.0F,
  });

  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    auto row = ui::column(
        {.out = &m_forecastRows[i],
         .align = FlexAlign::Stretch,
         .justify = FlexJustify::Center,
         .gap = Style::spaceXs * 0.5F * scale,
         .flexGrow = 1.0F,
         .configure = [scale](Flex& dayRow) { dayRow.setPadding(Style::spaceXs * scale, 0.0F); }}
    );

    auto daySlot = ui::row(
        {.out = &m_forecastIconSlots[i], .align = FlexAlign::Center, .gap = Style::spaceXs * scale, .flexGrow = 1.0F},
        ui::glyph({
            .out = &m_forecastGlyphs[i],
            .glyph = "weather-cloud",
            .glyphSize = Style::fontSizeBody * 1.2F * scale,
            .color = colorSpecFromRole(ColorRole::OnSurface),
        }),
        ui::label({
            .out = &m_forecastMetas[i],
            .text = i18n::tr("control-center.weather.forecast-placeholder.day"),
            .fontSize = Style::fontSizeBody * scale,
            .fontWeight = FontWeight::Bold,
            .color = colorSpecFromRole(ColorRole::OnSurface),
            .maxLines = 1,
        })
    );
    auto topRow = ui::row({
        .align = FlexAlign::Center,
        .justify = FlexJustify::SpaceBetween,
        .gap = Style::spaceSm * scale,
    });
    topRow->addChild(std::move(daySlot));

    topRow->addChild(
        ui::label({
            .out = &m_forecastTemps[i],
            .text = i18n::tr("control-center.weather.forecast-placeholder.temperature"),
            .fontSize = Style::fontSizeBody * scale,
            .color = colorSpecFromRole(ColorRole::OnSurface),
            .maxLines = 1,
            .textAlign = TextAlign::End,
        })
    );

    row->addChild(std::move(topRow));
    row->addChild(
        ui::label({
            .out = &m_forecastDescs[i],
            .text = i18n::tr("control-center.weather.forecast-placeholder.description"),
            .fontSize = Style::fontSizeCaption * scale,
            .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
            .maxLines = 1,
        })
    );

    auto hitArea = ui::inputArea({});
    hitArea->setAcceptedButtons(0);
    hitArea->setParticipatesInLayout(false);
    hitArea->setZIndex(2);
    m_forecastHitAreas[i] = static_cast<InputArea*>(row->addChild(std::move(hitArea)));

    forecastRowsContainer->addChild(std::move(row));

    if (i + 1 < kForecastRowCount) {
      forecastRowsContainer->addChild(
          ui::separator({
              .out = &m_forecastSeparators[i],
              .thickness = std::max(1.0F, scale),
          })
      );
    }
  }

  forecastColumn->addChild(std::move(forecastRowsContainer));
  tab->addChild(std::move(forecastColumn));
  return tab;
}

void WeatherTab::beginForecastSlideOut(ForecastView nextView) {
  const auto swap = [this, nextView]() {
    m_forecastView = nextView;
    PanelManager::instance().refresh();
  };
  const int direction = nextView == ForecastView::Hourly ? 1 : -1;
  if (!m_slide.slideOut(m_forecastRowsContainer, m_ui.forecastColumn, direction, swap)) {
    swap();
  }
}

void WeatherTab::doLayout(Renderer& renderer, float contentWidth, float bodyHeight) {
  if (m_ui.rootLayout == nullptr || m_ui.currentText == nullptr || m_ui.forecastColumn == nullptr) {
    return;
  }

  const bool slidingOut = m_slide.slidingOut();
  const float scale = contentScale();

  if (slidingOut) {
    m_ui.rootLayout->layout(renderer);
    return;
  }

  for (auto* label : m_forecastTemps) {
    if (label != nullptr) {
      label->setMaxWidth(0.0F);
      label->setMinWidth(0.0F);
    }
  }

  m_ui.rootLayout->setSize(contentWidth, bodyHeight);
  m_ui.rootLayout->layout(renderer);

  const float leftColumnWidth = m_ui.leftColumn != nullptr
      ? std::max(0.0F, m_ui.leftColumn->width() - (m_ui.leftColumn->paddingLeft() + m_ui.leftColumn->paddingRight()))
      : contentWidth;
  if (m_ui.currentCard != nullptr) {
    m_ui.currentCard->setMinWidth(leftColumnWidth);
  }
  if (m_ui.detailsCard != nullptr) {
    m_ui.detailsCard->setMinWidth(leftColumnWidth);
  }
  if (m_ui.statusLabel != nullptr) {
    m_ui.statusLabel->setMaxWidth(leftColumnWidth);
  }
  for (auto* label :
       {m_ui.feelsLikeLabel, m_ui.tempMinLabel, m_ui.tempMaxLabel, m_ui.windLabel, m_ui.sunriseLabel, m_ui.sunsetLabel,
        m_ui.elevationLabel, m_ui.uvIndexLabel, m_ui.timeZoneLabel}) {
    if (label != nullptr) {
      label->setMaxWidth(leftColumnWidth);
    }
  }

  if (m_ui.currentCard != nullptr) {
    m_ui.currentCard->setMinHeight(Style::controlHeightLg * 3.1F * scale);
  }
  if (m_ui.detailsCard != nullptr) {
    m_ui.detailsCard->setMinHeight(0.0F);
    m_ui.detailsCard->setFlexGrow(0.0F);
  }

  if (m_ui.currentGlyph != nullptr && m_ui.currentCard != nullptr) {
    const float cardInnerHeight = std::max(
        0.0F, m_ui.currentCard->height() - (m_ui.currentCard->paddingTop() + m_ui.currentCard->paddingBottom())
    );
    const float desiredGlyph =
        std::max(Style::controlHeightLg * 1.8F * scale, std::min(kCurrentGlyphSize * scale, cardInnerHeight * 0.8F));
    m_ui.currentGlyph->setGlyphSize(desiredGlyph);
  }

  std::size_t visibleForecastDays = 0;
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (m_forecastRows[i] != nullptr && m_forecastRows[i]->visible()) {
      ++visibleForecastDays;
    }
  }

  if (m_ui.forecastColumn != nullptr && visibleForecastDays > 0) {
    const float separatorThickness = std::max(1.0F, scale);
    std::size_t visibleSeparators = 0;
    for (auto* separator : m_forecastSeparators) {
      if (separator != nullptr) {
        separator->setThickness(separatorThickness);
        if (separator->visible()) {
          ++visibleSeparators;
        }
      }
    }
    const float forecastInnerHeight = std::max(
        0.0F, m_ui.forecastColumn->height() - (m_ui.forecastColumn->paddingTop() + m_ui.forecastColumn->paddingBottom())
    );
    const float pickerHeight = m_ui.forecastViewPicker != nullptr && m_ui.forecastViewPicker->visible()
        ? m_ui.forecastViewPicker->height()
        : 0.0F;
    const float separatorsTotal = separatorThickness * static_cast<float>(visibleSeparators);
    const float gapsTotal = m_ui.forecastColumn->gap() * static_cast<float>(visibleForecastDays + visibleSeparators);
    const float rowHeight = std::max(
        Style::controlHeightLg * scale,
        (forecastInnerHeight - pickerHeight - separatorsTotal - gapsTotal) / static_cast<float>(visibleForecastDays)
    );

    for (std::size_t i = 0; i < kForecastRowCount; ++i) {
      if (m_forecastRows[i] == nullptr) {
        continue;
      }
      m_forecastRows[i]->setMinHeight(m_forecastRows[i]->visible() ? rowHeight : 0.0F);
    }
  }

  float forecastTempColumnWidth = 0.0F;
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (m_forecastRows[i] != nullptr && m_forecastRows[i]->visible() && m_forecastTemps[i] != nullptr) {
      m_forecastTemps[i]->measure(renderer);
      forecastTempColumnWidth = std::max(forecastTempColumnWidth, m_forecastTemps[i]->width());
    }
  }

  const float forecastInnerWidth = m_ui.forecastColumn != nullptr
      ? std::max(
            0.0F,
            m_ui.forecastColumn->width() - m_ui.forecastColumn->paddingLeft() - m_ui.forecastColumn->paddingRight()
        )
      : 0.0F;
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (m_forecastRows[i] == nullptr || !m_forecastRows[i]->visible()) {
      continue;
    }
    // Stretch the row to the column's inner width so the topRow's SpaceBetween
    // pins every temperature to a common right edge instead of letting each row
    // size to its own content (which lets a wide row run past the panel edge).
    m_forecastRows[i]->setMinWidth(forecastInnerWidth);
    if (m_forecastTemps[i] != nullptr) {
      m_forecastTemps[i]->setMinWidth(forecastTempColumnWidth);
    }
    if (m_forecastMetas[i] != nullptr) {
      const float glyphWidth = m_forecastGlyphs[i] != nullptr ? m_forecastGlyphs[i]->width() : 0.0F;
      const float daySlotGap = m_forecastIconSlots[i] != nullptr ? m_forecastIconSlots[i]->gap() : 0.0F;
      const float topRowGap = Style::spaceSm * scale;
      const float metaMaxWidth = forecastInnerWidth - forecastTempColumnWidth - topRowGap - glyphWidth - daySlotGap;
      m_forecastMetas[i]->setMaxWidth(std::max(1.0F, metaMaxWidth));
    }
    if (m_forecastDescs[i] != nullptr) {
      m_forecastDescs[i]->setMaxWidth(std::max(1.0F, forecastInnerWidth));
    }
  }

  if (m_ui.locationPrompt != nullptr && m_ui.locationPrompt->visible() && m_ui.locationPromptBody != nullptr) {
    const float cardPadding =
        m_ui.currentCard != nullptr ? m_ui.currentCard->paddingLeft() + m_ui.currentCard->paddingRight() : 0.0F;
    const float glyphWidth = m_ui.locationPromptGlyph != nullptr ? m_ui.locationPromptGlyph->width() : 0.0F;
    const float promptGap = Style::spaceMd * scale;
    const float textWidth = std::max(1.0F, leftColumnWidth - cardPadding - glyphWidth - promptGap);
    m_ui.locationPromptBody->setMaxWidth(textWidth);
  }

  if (m_ui.effectNode != nullptr && m_ui.currentCard != nullptr) {
    m_ui.effectNode->setPosition(0.0F, 0.0F);
    m_ui.effectNode->setFrameSize(m_ui.currentCard->width(), m_ui.currentCard->height());
  }

  // The weather tab derives several width constraints from the first measurement
  // pass. Run layout again so the final geometry reflects those constraints
  // instead of keeping the placeholder/pre-constraint positions.
  m_ui.rootLayout->layout(renderer);

  if (m_ui.effectNode != nullptr && m_ui.currentCard != nullptr) {
    m_ui.effectNode->setFrameSize(m_ui.currentCard->width(), m_ui.currentCard->height());
  }

  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (m_forecastRows[i] == nullptr || m_forecastHitAreas[i] == nullptr) {
      continue;
    }
    const bool visible = m_forecastRows[i]->visible();
    m_forecastHitAreas[i]->setVisible(visible);
    m_forecastHitAreas[i]->setPosition(0.0F, 0.0F);
    m_forecastHitAreas[i]->setSize(
        visible ? m_forecastRows[i]->width() : 0.0F, visible ? m_forecastRows[i]->height() : 0.0F
    );
  }

  if (m_slide.pendingSlideIn()) {
    m_slide.slideIn(m_forecastRowsContainer, m_ui.forecastColumn);
  }
}

void WeatherTab::doUpdate(Renderer& renderer) { sync(renderer); }

void WeatherTab::setForecastVisibleRowCount(std::size_t count) {
  const std::size_t visibleCount = std::min(count, kForecastRowCount);
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (m_forecastRows[i] != nullptr) {
      m_forecastRows[i]->setVisible(i < visibleCount);
    }
    if (m_forecastHitAreas[i] != nullptr) {
      m_forecastHitAreas[i]->setVisible(i < visibleCount);
      if (i >= visibleCount) {
        m_forecastHitAreas[i]->clearTooltip();
      }
    }
    if (i + 1 < kForecastRowCount && m_forecastSeparators[i] != nullptr) {
      m_forecastSeparators[i]->setVisible(i + 1 < visibleCount);
    }
  }
}

void WeatherTab::showLocationPrompt(bool show) {
  if (m_ui.locationPrompt != nullptr) {
    m_ui.locationPrompt->setVisible(show);
  }
  if (m_ui.glyphColumn != nullptr) {
    m_ui.glyphColumn->setVisible(!show);
  }
  if (m_ui.currentText != nullptr) {
    m_ui.currentText->setVisible(!show);
  }
}

void WeatherTab::onClose() {
  m_ui = {};
  m_forecastRows.fill(nullptr);
  m_forecastSeparators.fill(nullptr);
  m_forecastIconSlots.fill(nullptr);
  m_forecastGlyphs.fill(nullptr);
  m_forecastMetas.fill(nullptr);
  m_forecastDescs.fill(nullptr);
  m_forecastTemps.fill(nullptr);
  m_forecastHitAreas.fill(nullptr);
  m_activeEffect = EffectType::None;
  m_shaderTime = 0.0F;
}

void WeatherTab::sync(Renderer& renderer) {
  if (m_ui.statusLabel == nullptr
      || m_ui.currentGlyph == nullptr
      || m_ui.currentTempLabel == nullptr
      || m_ui.currentDescLabel == nullptr
      || m_ui.updatedLabel == nullptr) {
    return;
  }

  showLocationPrompt(false);

  const bool showLocation = m_config == nullptr || m_config->config().shell.showLocation;
  if (m_ui.updatedLabel != nullptr) {
    m_ui.updatedLabel->setVisible(showLocation);
  }
  if (m_ui.timeZoneRow != nullptr) {
    m_ui.timeZoneRow->setVisible(showLocation);
  }

  if (m_weather == nullptr || !m_weather->enabled()) {
    m_ui.currentGlyph->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
    m_ui.currentTempLabel->setText("--°C");
    if (m_ui.currentHiLoLabel != nullptr) {
      m_ui.currentHiLoLabel->setText("-- / --");
    }
    m_ui.currentDescLabel->setText(i18n::tr("control-center.weather.disabled"));
    m_ui.updatedLabel->setText(i18n::tr("control-center.weather.location-unavailable"));
    m_ui.updatedLabel->setVisible(false);
    m_ui.statusLabel->setText("");
    m_ui.statusLabel->setVisible(false);
    if (m_ui.windLabel != nullptr) {
      m_ui.windLabel->setText("--");
    }
    if (m_ui.sunriseLabel != nullptr) {
      m_ui.sunriseLabel->setText("--");
    }
    if (m_ui.sunsetLabel != nullptr) {
      m_ui.sunsetLabel->setText("--");
    }
    if (m_ui.tempMaxLabel != nullptr) {
      m_ui.tempMaxLabel->setText("--");
    }
    if (m_ui.tempMinLabel != nullptr) {
      m_ui.tempMinLabel->setText("--");
    }
    if (m_ui.feelsLikeLabel != nullptr) {
      m_ui.feelsLikeLabel->setText("--");
    }
    if (m_ui.elevationLabel != nullptr) {
      m_ui.elevationLabel->setText("--");
    }
    if (m_ui.uvIndexLabel != nullptr) {
      m_ui.uvIndexLabel->setText("--");
    }
    if (m_ui.timeZoneLabel != nullptr) {
      m_ui.timeZoneLabel->setText("--");
    }
    setForecastVisibleRowCount(0);
    hideEffect();
    return;
  }

  if (!m_weather->locationConfigured()) {
    showLocationPrompt(true);
    if (m_ui.windLabel != nullptr) {
      m_ui.windLabel->setText("--");
    }
    if (m_ui.sunriseLabel != nullptr) {
      m_ui.sunriseLabel->setText("--");
    }
    if (m_ui.sunsetLabel != nullptr) {
      m_ui.sunsetLabel->setText("--");
    }
    if (m_ui.tempMaxLabel != nullptr) {
      m_ui.tempMaxLabel->setText("--");
    }
    if (m_ui.tempMinLabel != nullptr) {
      m_ui.tempMinLabel->setText("--");
    }
    if (m_ui.feelsLikeLabel != nullptr) {
      m_ui.feelsLikeLabel->setText("--");
    }
    if (m_ui.elevationLabel != nullptr) {
      m_ui.elevationLabel->setText("--");
    }
    if (m_ui.uvIndexLabel != nullptr) {
      m_ui.uvIndexLabel->setText("--");
    }
    if (m_ui.timeZoneLabel != nullptr) {
      m_ui.timeZoneLabel->setText("--");
    }
    setForecastVisibleRowCount(0);
    hideEffect();
    return;
  }

  const auto& snapshot = m_weather->snapshot();
  if (!snapshot.valid) {
    m_ui.currentGlyph->setColor(colorSpecFromRole(ColorRole::OnSurfaceVariant));
    m_ui.currentTempLabel->setText(std::format("--{}", m_weather->displayTemperatureUnit()));
    if (m_ui.currentHiLoLabel != nullptr) {
      m_ui.currentHiLoLabel->setText("-- / --");
    }
    m_ui.currentDescLabel->setText(
        m_weather->loading() ? i18n::tr("control-center.weather.fetching")
                             : i18n::tr("control-center.weather.data-unavailable")
    );
    m_ui.updatedLabel->setText(
        snapshot.locationName.empty() ? i18n::tr("location.locations.current") : snapshot.locationName
    );
    m_ui.updatedLabel->setVisible(false);
    m_ui.statusLabel->setText(m_weather->error());
    m_ui.statusLabel->setVisible(!m_weather->error().empty());
    if (m_ui.windLabel != nullptr) {
      m_ui.windLabel->setText("--");
    }
    if (m_ui.sunriseLabel != nullptr) {
      m_ui.sunriseLabel->setText("--");
    }
    if (m_ui.sunsetLabel != nullptr) {
      m_ui.sunsetLabel->setText("--");
    }
    if (m_ui.tempMaxLabel != nullptr) {
      m_ui.tempMaxLabel->setText("--");
    }
    if (m_ui.tempMinLabel != nullptr) {
      m_ui.tempMinLabel->setText("--");
    }
    if (m_ui.feelsLikeLabel != nullptr) {
      m_ui.feelsLikeLabel->setText("--");
    }
    if (m_ui.elevationLabel != nullptr) {
      m_ui.elevationLabel->setText("--");
    }
    if (m_ui.uvIndexLabel != nullptr) {
      m_ui.uvIndexLabel->setText("--");
    }
    if (m_ui.timeZoneLabel != nullptr) {
      m_ui.timeZoneLabel->setText("--");
    }
    setForecastVisibleRowCount(0);
    hideEffect();
    return;
  }

  m_ui.currentGlyph->setGlyph(WeatherService::glyphForCode(snapshot.current.weatherCode, snapshot.current.isDay));
  m_ui.currentGlyph->setColor(colorSpecFromRole(snapshot.current.isDay ? ColorRole::Primary : ColorRole::Secondary));
  m_ui.currentTempLabel->setText(
      std::format(
          "{}{}", static_cast<int>(std::lround(m_weather->displayTemperature(snapshot.current.temperatureC))),
          m_weather->displayTemperatureUnit()
      )
  );
  if (m_ui.currentHiLoLabel != nullptr) {
    if (!snapshot.forecastDays.empty()) {
      m_ui.currentHiLoLabel->setText(
          std::format(
              "{} / {}{}",
              static_cast<int>(
                  std::lround(m_weather->displayTemperature(snapshot.forecastDays.front().temperatureMinC))
              ),
              static_cast<int>(
                  std::lround(m_weather->displayTemperature(snapshot.forecastDays.front().temperatureMaxC))
              ),
              m_weather->displayTemperatureUnit()
          )
      );
    } else {
      m_ui.currentHiLoLabel->setText("-- / --");
    }
  }
  m_ui.currentDescLabel->setText(WeatherService::descriptionForCode(snapshot.current.weatherCode));
  m_ui.updatedLabel->setText(
      snapshot.locationName.empty() ? i18n::tr("location.locations.current") : snapshot.locationName
  );
  m_ui.updatedLabel->setVisible(showLocation);
  const std::string status = m_weather->loading() ? i18n::tr("control-center.weather.refreshing")
                                                  : (snapshot.valid ? std::string{} : m_weather->error());
  m_ui.statusLabel->setText(status);
  m_ui.statusLabel->setColor(
      colorSpecFromRole(m_weather->error().empty() ? ColorRole::OnSurfaceVariant : ColorRole::Error)
  );
  m_ui.statusLabel->setVisible(!status.empty());
  if (m_ui.windLabel != nullptr) {
    const bool imperial = m_weather->useImperial();
    const double windSpeed = imperial ? snapshot.current.windSpeedKmh * 0.621371 : snapshot.current.windSpeedKmh;
    const char* windUnit =
        imperial ? "mph" : (snapshot.currentUnits.windSpeed.empty() ? "km/h" : snapshot.currentUnits.windSpeed.c_str());
    m_ui.windLabel->setText(
        std::format(
            "{} {} {}", static_cast<int>(std::lround(windSpeed)), windUnit,
            windDirectionLabel(snapshot.current.windDirectionDeg)
        )
    );
  }
  if (m_ui.sunriseLabel != nullptr) {
    const auto& fmt = m_config->config().shell.timeFormat;
    m_ui.sunriseLabel->setText(
        !snapshot.forecastDays.empty() ? formatIsoTime(snapshot.forecastDays.front().sunriseIso, fmt.c_str())
                                       : std::string("--")
    );
  }
  if (m_ui.sunsetLabel != nullptr) {
    const auto& fmt = m_config->config().shell.timeFormat;
    m_ui.sunsetLabel->setText(
        !snapshot.forecastDays.empty() ? formatIsoTime(snapshot.forecastDays.front().sunsetIso, fmt.c_str())
                                       : std::string("--")
    );
  }
  auto unit = m_weather->displayTemperatureUnit();
  if (m_ui.feelsLikeLabel != nullptr) {
    const auto& apparent = snapshot.current.apparentTemperatureC;
    m_ui.feelsLikeLabel->setText(
        apparent.has_value()
            ? std::format("{}{}", static_cast<int>(std::lround(m_weather->displayTemperature(*apparent))), unit)
            : std::string("--")
    );
  }
  if (m_ui.tempMinLabel != nullptr) {
    if (!snapshot.forecastDays.empty()) {
      const int temp =
          static_cast<int>(std::lround(m_weather->displayTemperature(snapshot.forecastDays.front().temperatureMinC)));
      m_ui.tempMinLabel->setText(std::format("{}{}", temp, unit));
    } else {
      m_ui.tempMinLabel->setText("--");
    }
  }
  if (m_ui.tempMaxLabel != nullptr) {
    if (!snapshot.forecastDays.empty()) {
      const int temp =
          static_cast<int>(std::lround(m_weather->displayTemperature(snapshot.forecastDays.front().temperatureMaxC)));
      m_ui.tempMaxLabel->setText(std::format("{}{}", temp, unit));
    } else {
      m_ui.tempMaxLabel->setText("--");
    }
  }
  if (m_ui.elevationLabel != nullptr) {
    const bool imperial = m_weather->useImperial();
    const int elevation = static_cast<int>(imperial ? snapshot.elevationM * 3.28084 : snapshot.elevationM);
    m_ui.elevationLabel->setText(std::format("{}{}", elevation, imperial ? "ft" : "m"));
  }
  if (m_ui.uvIndexLabel != nullptr) {
    m_ui.uvIndexLabel->setText(std::format("{:.1F}", snapshot.current.uvIndex));
  }
  if (m_ui.timeZoneLabel != nullptr) {
    // Use the last component of the IANA path ("America/Toronto" → "Toronto") to keep
    // the label short enough to remain right-aligned without elision in most cases.
    std::string tzCity = snapshot.timezone;
    if (const auto slash = tzCity.rfind('/'); slash != std::string::npos) {
      tzCity = tzCity.substr(slash + 1);
    }
    m_ui.timeZoneLabel->setText(
        snapshot.timezoneAbbreviation.empty() ? (snapshot.timezone.empty() ? std::string("--") : snapshot.timezone)
                                              : std::format("{} ({})", snapshot.timezoneAbbreviation, tzCity)
    );
  }

  if (m_ui.forecastViewPicker != nullptr) {
    m_ui.forecastViewPicker->setSelectedIndex(static_cast<std::size_t>(m_forecastView));
  }
  if (m_forecastView == ForecastView::Hourly) {
    syncHourlyForecast(renderer, snapshot);
  } else {
    syncDailyForecast(renderer, snapshot);
  }

  if (m_ui.effectNode != nullptr) {
    const EffectType newEffect = kTestEffect != EffectType::None
        ? kTestEffect
        : (m_weather->effectsEnabled() ? effectForWeatherCode(snapshot.current.weatherCode, snapshot.current.isDay)
                                       : EffectType::None);
    if (newEffect != m_activeEffect) {
      m_activeEffect = newEffect;
      m_shaderTime = 0.0F;
    }
    m_ui.effectNode->setEffectType(m_activeEffect);
    m_ui.effectNode->setBgColor(colorForRole(ColorRole::Surface));
    m_ui.effectNode->setRadius(Style::scaledRadiusXl(contentScale()));
    m_ui.effectNode->setVisible(m_activeEffect != EffectType::None);
  }
}

void WeatherTab::syncDailyForecast(Renderer& renderer, const WeatherSnapshot& snapshot) {
  const bool firstForecastIsToday =
      !snapshot.forecastDays.empty() && snapshot.forecastDays.front().dateIso == todayIso(snapshot.utcOffsetSeconds);
  const std::size_t forecastStart = firstForecastIsToday ? 1 : 0;
  const std::size_t visibleForecastCount = forecastStart < snapshot.forecastDays.size()
      ? std::min(kForecastRowCount, snapshot.forecastDays.size() - forecastStart)
      : 0;

  setForecastVisibleRowCount(visibleForecastCount);
  const std::string timeFormat = m_config != nullptr && !m_config->config().shell.timeFormat.empty()
      ? m_config->config().shell.timeFormat
      : std::string("%H:%M");
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (i >= visibleForecastCount) {
      continue;
    }

    const auto& day = snapshot.forecastDays[i + forecastStart];
    const std::string condition = WeatherService::shortDescriptionForCode(day.weatherCode);
    const std::string tempHigh = std::format(
        "{}{}", static_cast<int>(std::lround(m_weather->displayTemperature(day.temperatureMaxC))),
        m_weather->displayTemperatureUnit()
    );
    const std::string tempLow = std::format(
        "{}{}", static_cast<int>(std::lround(m_weather->displayTemperature(day.temperatureMinC))),
        m_weather->displayTemperatureUnit()
    );
    const std::string sunrise =
        day.sunriseIso.empty() ? std::string("--") : formatIsoTime(day.sunriseIso, timeFormat.c_str());
    const std::string sunset =
        day.sunsetIso.empty() ? std::string("--") : formatIsoTime(day.sunsetIso, timeFormat.c_str());
    const std::vector<TooltipRow> tooltipRows{
        {i18n::tr("control-center.weather.daily.tooltip.condition"), condition},
        {i18n::tr("control-center.weather.daily.tooltip.low"), tempLow},
        {i18n::tr("control-center.weather.daily.tooltip.high"), tempHigh},
        {i18n::tr("control-center.weather.daily.tooltip.sunrise"), sunrise},
        {i18n::tr("control-center.weather.daily.tooltip.sunset"), sunset},
    };
    if (m_forecastHitAreas[i] != nullptr) {
      m_forecastHitAreas[i]->setTooltip(tooltipRows);
    }
    if (m_forecastGlyphs[i] != nullptr) {
      m_forecastGlyphs[i]->setGlyph(WeatherService::glyphForCode(day.weatherCode, true));
      m_forecastGlyphs[i]->setColor(colorSpecFromRole(ColorRole::Primary));
      m_forecastGlyphs[i]->measure(renderer);
    }
    if (m_forecastMetas[i] != nullptr) {
      m_forecastMetas[i]->setText(weekdayLabel(day.dateIso));
      m_forecastMetas[i]->clearTooltip();
      m_forecastMetas[i]->measure(renderer);
    }
    if (m_forecastTemps[i] != nullptr) {
      m_forecastTemps[i]->setText(std::format("{} / {}", tempLow, tempHigh));
      m_forecastTemps[i]->clearTooltip();
      m_forecastTemps[i]->measure(renderer);
    }
    if (m_forecastDescs[i] != nullptr) {
      m_forecastDescs[i]->setText(condition);
      m_forecastDescs[i]->clearTooltip();
      m_forecastDescs[i]->measure(renderer);
    }
  }
}

void WeatherTab::syncHourlyForecast(Renderer& renderer, const WeatherSnapshot& snapshot) {
  const std::size_t visibleForecastCount = std::min(kForecastRowCount, snapshot.forecastHours.size());
  const bool imperial = m_weather != nullptr && m_weather->useImperial();
  const char* windUnit =
      imperial ? "mph" : (snapshot.hourlyUnits.windSpeed.empty() ? "km/h" : snapshot.hourlyUnits.windSpeed.c_str());
  const std::string timeFormat = m_config != nullptr && !m_config->config().shell.timeFormat.empty()
      ? m_config->config().shell.timeFormat
      : std::string("%H:%M");

  setForecastVisibleRowCount(visibleForecastCount);
  for (std::size_t i = 0; i < kForecastRowCount; ++i) {
    if (i >= visibleForecastCount) {
      continue;
    }

    const auto& hour = snapshot.forecastHours[i];
    const int displayTemp = static_cast<int>(std::lround(m_weather->displayTemperature(hour.temperatureC)));
    const double displayWind = imperial ? hour.windSpeedKmh * 0.621371 : hour.windSpeedKmh;
    const std::string displayWindText = std::format("{} {}", static_cast<int>(std::lround(displayWind)), windUnit);
    const std::string condition = WeatherService::shortDescriptionForCode(hour.weatherCode);
    const std::vector<TooltipRow> tooltipRows{
        {i18n::tr("control-center.weather.hourly.tooltip.condition"), condition},
        {i18n::tr("control-center.weather.hourly.tooltip.rain"),
         std::format("{}%", hour.precipitationProbabilityPercent)},
        {i18n::tr("control-center.weather.hourly.tooltip.humidity"), std::format("{}%", hour.relativeHumidityPercent)},
        {i18n::tr("control-center.weather.hourly.tooltip.wind"), displayWindText},
    };
    if (m_forecastHitAreas[i] != nullptr) {
      m_forecastHitAreas[i]->setTooltip(tooltipRows);
    }
    if (m_forecastGlyphs[i] != nullptr) {
      m_forecastGlyphs[i]->setGlyph(WeatherService::glyphForCode(hour.weatherCode, hour.isDay));
      m_forecastGlyphs[i]->setColor(colorSpecFromRole(hour.isDay ? ColorRole::Primary : ColorRole::Secondary));
      m_forecastGlyphs[i]->measure(renderer);
    }
    if (m_forecastMetas[i] != nullptr) {
      m_forecastMetas[i]->setText(hourLabel(hour.timeIso, timeFormat));
      m_forecastMetas[i]->clearTooltip();
      m_forecastMetas[i]->measure(renderer);
    }
    if (m_forecastTemps[i] != nullptr) {
      m_forecastTemps[i]->setText(std::format("{}{}", displayTemp, m_weather->displayTemperatureUnit()));
      m_forecastTemps[i]->clearTooltip();
      m_forecastTemps[i]->measure(renderer);
    }
    if (m_forecastDescs[i] != nullptr) {
      m_forecastDescs[i]->setText(
          i18n::tr(
              "control-center.weather.hourly.summary", "condition", condition, "precip",
              hour.precipitationProbabilityPercent
          )
      );
      m_forecastDescs[i]->clearTooltip();
      m_forecastDescs[i]->measure(renderer);
    }
  }
}

std::string WeatherTab::todayIso(std::int32_t utcOffsetSeconds) {
  const auto now = std::chrono::system_clock::now() + std::chrono::seconds{utcOffsetSeconds};
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&time, &tm);

  return formatStrftime("%Y-%m-%d", tm);
}

std::string WeatherTab::hourLabel(const std::string& isoTime, const std::string& timeFormat) {
  if (isoTime.size() < 16) {
    return isoTime;
  }
  const std::string formatted = formatIsoTime(isoTime, timeFormat.empty() ? "%H:%M" : timeFormat.c_str());
  return formatted.empty() ? isoTime : formatted;
}

std::string WeatherTab::weekdayLabel(const std::string& isoDate) {
  if (isoDate.size() != 10) {
    return isoDate;
  }

  std::tm tm{};
  tm.tm_year = std::stoi(isoDate.substr(0, 4)) - 1900;
  tm.tm_mon = std::stoi(isoDate.substr(5, 2)) - 1;
  tm.tm_mday = std::stoi(isoDate.substr(8, 2));
  if (std::mktime(&tm) == -1) {
    return isoDate;
  }

  const std::string weekday = formatStrftime("%A", tm);
  if (weekday.empty()) {
    return isoDate;
  }
  return weekday;
}

void WeatherTab::hideEffect() {
  m_activeEffect = EffectType::None;
  m_shaderTime = 0.0F;
  if (m_ui.effectNode != nullptr) {
    m_ui.effectNode->setEffectType(EffectType::None);
    m_ui.effectNode->setVisible(false);
  }
}

void WeatherTab::onFrameTick(float deltaMs) {
  if (m_ui.effectNode == nullptr || !m_ui.effectNode->visible() || m_activeEffect == EffectType::None) {
    return;
  }
  m_shaderTime += deltaMs * 0.001F;
  m_ui.effectNode->setTime(m_shaderTime);
}

EffectType WeatherTab::effectForWeatherCode(std::int32_t code, bool isDay) {
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    return EffectType::Rain;
  }
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    return EffectType::Snow;
  }
  if (code == 3) {
    return EffectType::Cloud;
  }
  if (code >= 40 && code <= 49) {
    return EffectType::Fog;
  }
  if (code == 0 && isDay) {
    return EffectType::Sun;
  }
  if (code == 0 && !isDay) {
    return EffectType::Stars;
  }
  return EffectType::None;
}
