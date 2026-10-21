#pragma once

#include "core/timer_manager.h"
#include "render/animation/animation_manager.h"
#include "shell/control_center/slide_transition.h"
#include "shell/control_center/tab.h"
#include "ui/controls/calendar_view.h"

#include <limits>

class Button;
class CalendarService;
class ConfigService;
class InputArea;
class Label;
class ScrollView;

class CalendarTab : public Tab {
public:
  explicit CalendarTab(ConfigService* config = nullptr, CalendarService* calendar = nullptr);

  std::unique_ptr<Flex> create() override;
  std::unique_ptr<Flex> createHeaderActions() override;
  void setActive(bool active) override;
  void onClose() override;

private:
  void focusToday();
  void changeMonthBy(int delta);
  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;
  void rebuild();
  void rebuildEventList(float scale);
  void toggleEventsCard();

  ConfigService* m_config = nullptr;
  CalendarService* m_calendar = nullptr;
  bool m_changeCallbackRegistered = false;
  bool m_eventsDirty = false;
  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* rootLayout = nullptr;
    Button* toggleEventsCardButton = nullptr;
    InputArea* calendarArea = nullptr;
    Flex* card = nullptr;
    Flex* header = nullptr;
    Flex* previousSlot = nullptr;
    Flex* nextSlot = nullptr;
    Flex* monthWrap = nullptr;
    Label* todayLabel = nullptr;
    Label* monthLabel = nullptr;
    Button* previousButton = nullptr;
    Button* nextButton = nullptr;
    Flex* gridViewport = nullptr;
    Flex* grid = nullptr;
    Flex* eventsCard = nullptr;
    Label* eventsTitle = nullptr;
    ScrollView* eventsScroll = nullptr;
  };
  Ui m_ui;
  calendar_view::EventListState m_eventListState;
  int m_selectedYear = std::numeric_limits<int>::min();
  int m_selectedMonth = -1;
  int m_selectedDay = -1;
  int m_monthOffset = 0;
  float m_lastInnerWidth = -1.0F;
  float m_lastInnerHeight = -1.0F;
  int m_lastDisplayYear = std::numeric_limits<int>::min();
  int m_lastDisplayMonth = -1;
  int m_lastCurrentYear = std::numeric_limits<int>::min();
  int m_lastCurrentMonth = -1;
  int m_lastToday = -1;
  SlideTransition m_slide;
  bool m_showEventsCard = true;
  bool m_showWeekNumbers = false;
  Timer m_eventFadeTimer;
};
