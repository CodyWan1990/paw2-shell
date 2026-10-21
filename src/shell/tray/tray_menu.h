#pragma once

#include "core/timer_manager.h"
#include "dbus/tray/tray_service.h"
#include "render/scene/input_dispatcher.h"
#include "render/scene/node.h"
#include "ui/controls/context_menu.h"
#include "ui/controls/scroll_view.h"
#include "ui/popup_chrome.h"
#include "wayland/parent_keyboard_grab.h"
#include "wayland/popup_surface.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ConfigService;
class RenderContext;
class WaylandConnection;
struct KeyboardEvent;
struct PointerEvent;
struct wl_surface;

class TrayMenu {
public:
  TrayMenu() = default;

  void initialize(WaylandConnection& wayland, ConfigService* config, TrayService* tray, RenderContext* renderContext);
  void onTrayChanged();

  void toggleForItem(const std::string& itemId, float contentScale = 1.0F);
  void close();
  void onFontChanged();
  void onThemeChanged();
  void requestLayout();
  void setClosedCallback(std::function<void()> callback);
  [[nodiscard]] bool isOpen() const noexcept { return m_visible; }

  [[nodiscard]] bool onPointerEvent(const PointerEvent& event);
  // Consumes keys while the menu is open (modal grab); Cancel closes it.
  [[nodiscard]] bool onKeyboardEvent(const KeyboardEvent& event);

private:
  struct MenuInstance {
    wl_output* output = nullptr;
    std::unique_ptr<PopupSurface> surface;
    std::unique_ptr<Node> sceneRoot;
    popup_chrome::Geometry chrome;
    InputDispatcher inputDispatcher;
    wl_surface* wlSurface = nullptr;
    bool pointerInside = false;
    ContextSubmenuDirection submenuDirection = ContextSubmenuDirection::Right;
    ScrollViewState scrollState;
  };

  void refreshEntries();
  void scheduleEntryRetry(int attempt);
  [[nodiscard]] uint32_t menuHeightPx(const std::vector<TrayMenuEntry>& menuEntries) const;
  [[nodiscard]] bool ownsSurface(wl_surface* surface) const;
  [[nodiscard]] float contentScale() const noexcept;
  [[nodiscard]] float menuWidth() const noexcept;
  void ensureSurface();
  void resizeMainSurfaceToEntries();
  void destroySurface();
  void rebuildScenes();
  void prepareMenuFrame(std::optional<std::size_t> submenuLevel, MenuInstance& inst, bool needsLayout);
  void buildMenuScene(std::optional<std::size_t> submenuLevel, MenuInstance& inst, uint32_t width, uint32_t height);
  void openSubmenu(std::int32_t parentEntryId, float rowCenterY);
  void openSubmenuAtLevel(std::size_t levelIndex, std::int32_t parentEntryId, float rowCenterY);
  void closeSubmenu();
  void closeSubmenusFrom(std::size_t levelIndex);
  [[nodiscard]] std::optional<TrayItemInfo> activeTrayItem() const;
  [[nodiscard]] bool activeItemPinned() const;
  bool toggleActiveItemPinned();

  WaylandConnection* m_wayland = nullptr;
  ConfigService* m_config = nullptr;
  TrayService* m_tray = nullptr;
  RenderContext* m_renderContext = nullptr;

  std::string m_activeItemId;
  std::vector<TrayMenuEntry> m_entries;
  std::unique_ptr<MenuInstance> m_instance;

  // The bar under the menu takes keyboard focus while the menu is open.
  ParentKeyboardGrab m_parentKeyboard;
  float m_contentScale = 1.0F;
  bool m_visible = false;
  std::string m_lastClosedItemId;
  std::chrono::steady_clock::time_point m_lastCloseTime;

  struct SubmenuLevel {
    std::vector<TrayMenuEntry> entries;
    std::int32_t parentEntryId = 0;
    std::int32_t pendingParentEntryId = 0;
    float pendingRowCenterY = 0.0F;
    std::unique_ptr<MenuInstance> instance;
  };
  std::vector<SubmenuLevel> m_submenuLevels;

  // Hyprland-only: keeps the popup surfaces in the focus whitelist so motion
  // events (hover) reach the popup eagerly instead of waiting for a click to
  // transfer focus from the bar's OnDemand layer surface.

  Timer m_retryTimer;
  std::function<void()> m_closedCallback;
};
