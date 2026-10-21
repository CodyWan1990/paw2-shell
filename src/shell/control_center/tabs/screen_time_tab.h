#pragma once

#include "render/animation/animation_manager.h"
#include "shell/control_center/slide_transition.h"
#include "shell/control_center/tab.h"
#include "ui/signal.h"

#include <array>
#include <string>
#include <vector>

class Box;
class Button;
class Flex;
class InputArea;
class Glyph;
class Image;
class Label;
class Renderer;
class ScreenTimeService;
class ScrollView;
class Segmented;

class ScreenTimeTab : public Tab {
public:
  explicit ScreenTimeTab(ScreenTimeService* screenTime);

  std::unique_ptr<Flex> create() override;
  void onClose() override;
  void setActive(bool active) override;

private:
  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;
  void onPanelCardOpacityChanged(float opacity) override;
  void syncContent(Renderer& renderer);
  void bindAppNameMaxWidths(Renderer& renderer, float gridWidth);
  void layoutChart(Renderer& renderer);
  void layoutAppRows(Renderer& renderer);
  void beginRangeSlideOut(int nextRangeDays);
  void syncEnabledUi();
  void syncDayLabelHover();
  void openDayDetail(std::size_t bucketIndex);
  void clearDayDetail();
  [[nodiscard]] std::string resolveIconPath(const std::string& appKey) const;

  ScreenTimeService* m_screenTime = nullptr;
  bool m_active = false;
  int m_rangeDays = 1;
  std::string m_detailDayKey;
  std::vector<std::string> m_bucketDayKeys;
  std::string m_lastSnapshotKey;
  SlideTransition m_slide;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* root = nullptr;
    Flex* usageCard = nullptr;
    Flex* detailBar = nullptr;
    Label* disabledLabel = nullptr;
    Label* detailTitle = nullptr;
    Segmented* rangePicker = nullptr;
    ScrollView* scroll = nullptr;
    Flex* chartPlotRow = nullptr;
    Flex* chartLabelRow = nullptr;
    Flex* mostUsedSection = nullptr;
    Label* totalLabel = nullptr;
    Label* emptyLabel = nullptr;
  };
  Ui m_ui;

  static constexpr int kMaxBuckets = 24;
  static constexpr int kMaxAppRows = 12;
  static constexpr int kMaxChartSeries = 5;
  static constexpr int kAppsPerRow = 2;
  static constexpr int kMostUsedRows = (kMaxAppRows + kAppsPerRow - 1) / kAppsPerRow;

  struct BucketColumnWidgets {
    Flex* plotColumn = nullptr;
    Box* track = nullptr;
    Label* label = nullptr;
    InputArea* labelHit = nullptr;
    Flex* labelCell = nullptr;
    std::array<InputArea*, kMaxChartSeries> segmentHits{};
    std::array<Box*, kMaxChartSeries> segments{};
    std::array<float, kMaxChartSeries> segmentHeights{};
  };
  std::array<BucketColumnWidgets, kMaxBuckets> m_bucketColumns{};

  struct AppRowWidgets {
    Flex* cell = nullptr;
    Flex* row = nullptr;
    Box* chartSwatch = nullptr;
    Flex* iconSlot = nullptr;
    Image* icon = nullptr;
    Glyph* iconFallback = nullptr;
    Label* name = nullptr;
    Label* duration = nullptr;
    Flex* barHost = nullptr;
    Box* barTrack = nullptr;
    Box* barFill = nullptr;
    float barFillRatio = 0.0F;
    std::string iconPath;
  };
  std::array<AppRowWidgets, kMaxAppRows> m_appRows{};
  std::array<Flex*, kMostUsedRows> m_appGridRows{};
  Signal<>::ScopedConnection m_paletteConn;

  void updateIconForRow(Renderer& renderer, AppRowWidgets& widgets, const std::string& appKey, float scale);
};
