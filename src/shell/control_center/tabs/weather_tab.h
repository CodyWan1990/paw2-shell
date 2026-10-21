#pragma once

#include "render/animation/animation_manager.h"
#include "render/core/render_styles.h"
#include "shell/control_center/slide_transition.h"
#include "shell/control_center/tab.h"

#include <array>
#include <cstdint>

class EffectNode;
class Flex;
class Glyph;
class InputArea;
class Label;
class Separator;
class Segmented;
class WeatherService;
class ConfigService;
struct WeatherSnapshot;

class WeatherTab : public Tab {
public:
  WeatherTab(WeatherService* weather, ConfigService* config);

  std::unique_ptr<Flex> create() override;
  void onClose() override;
  void onFrameTick(float deltaMs) override;

private:
  enum class ForecastView : std::uint8_t {
    Daily = 0,
    Hourly = 1,
  };

  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;
  void sync(Renderer& renderer);
  void beginForecastSlideOut(ForecastView nextView);
  void setForecastVisibleRowCount(std::size_t count);
  void syncDailyForecast(Renderer& renderer, const WeatherSnapshot& snapshot);
  void syncHourlyForecast(Renderer& renderer, const WeatherSnapshot& snapshot);
  void showLocationPrompt(bool show);
  void hideEffect();
  [[nodiscard]] static std::string todayIso(std::int32_t utcOffsetSeconds);
  [[nodiscard]] static std::string hourLabel(const std::string& isoTime, const std::string& timeFormat);
  [[nodiscard]] static std::string weekdayLabel(const std::string& isoDate);
  [[nodiscard]] static EffectType effectForWeatherCode(std::int32_t code, bool isDay);

  static constexpr std::size_t kForecastRowCount = 7;

  WeatherService* m_weather = nullptr;
  ConfigService* m_config = nullptr;
  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* rootLayout = nullptr;
    Flex* leftColumn = nullptr;
    Flex* currentCard = nullptr;
    Flex* glyphColumn = nullptr;
    Flex* detailsCard = nullptr;
    Flex* currentText = nullptr;
    Flex* locationPrompt = nullptr;
    Glyph* locationPromptGlyph = nullptr;
    Label* locationPromptBody = nullptr;
    Flex* forecastColumn = nullptr;
    Segmented* forecastViewPicker = nullptr;
    Label* statusLabel = nullptr;
    Glyph* currentGlyph = nullptr;
    Label* currentTempLabel = nullptr;
    Label* currentHiLoLabel = nullptr;
    Label* currentDescLabel = nullptr;
    Label* updatedLabel = nullptr;
    Label* windLabel = nullptr;
    Label* sunriseLabel = nullptr;
    Label* sunsetLabel = nullptr;
    Label* tempMaxLabel = nullptr;
    Label* tempMinLabel = nullptr;
    Label* feelsLikeLabel = nullptr;
    Label* elevationLabel = nullptr;
    Label* timeZoneLabel = nullptr;
    Flex* timeZoneRow = nullptr;
    Label* uvIndexLabel = nullptr;
    EffectNode* effectNode = nullptr;
  };
  Ui m_ui;
  Flex* m_forecastRowsContainer = nullptr;
  std::array<Flex*, kForecastRowCount> m_forecastRows{};
  std::array<Separator*, kForecastRowCount - 1> m_forecastSeparators{};
  std::array<Flex*, kForecastRowCount> m_forecastIconSlots{};
  std::array<Glyph*, kForecastRowCount> m_forecastGlyphs{};
  std::array<Label*, kForecastRowCount> m_forecastMetas{};
  std::array<Label*, kForecastRowCount> m_forecastDescs{};
  std::array<Label*, kForecastRowCount> m_forecastTemps{};
  std::array<InputArea*, kForecastRowCount> m_forecastHitAreas{};
  EffectType m_activeEffect = EffectType::None;
  ForecastView m_forecastView = ForecastView::Daily;
  SlideTransition m_slide;
  float m_shaderTime = 0.0F;
};
