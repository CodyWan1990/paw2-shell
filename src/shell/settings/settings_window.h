#pragma once

#include "compositors/paw2/paw2_runtime.h"
#include "core/timer_manager.h"
#include "render/animation/animation_manager.h"
#include "render/scene/input_dispatcher.h"
#include "render/scene/node.h"
#include "shell/settings/config_export_dialog_modal.h"
#include "shell/settings/search_picker_popup.h"
#include "shell/settings/settings_control_factory.h"
#include "shell/settings/settings_modal_host.h"
#include "shell/settings/settings_registry.h"
#include "shell/settings/settings_sheet_modal.h"
#include "shell/settings/widget_add_popup.h"
#include "ui/controls/context_menu_popup.h"
#include "ui/controls/roving_list_nav.h"
#include "ui/controls/scroll_view.h"
#include "ui/controls/select_dropdown_popup.h"
#include "ui/dialogs/layer_popup_host.h"
#include "ui/signal.h"
#include "wayland/toplevel_surface.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Box;
class AsyncTextureCache;
class Button;
class AccountsService;
class CalendarService;
class ClipboardService;
class IpcService;
class ConfigService;
class CompositorPlatform;
class ColorPickerDialogPresenter;
class DependencyService;
class FileDialogPresenter;
class Flex;
class GlyphPickerDialogPresenter;
class IdleManager;
class Input;
class Label;
class RenderContext;
class ThumbnailService;
class UPowerService;
class WaylandConnection;
struct KeyboardEvent;
struct PointerEvent;
struct wl_output;
struct wl_surface;
struct zwp_keyboard_shortcuts_inhibitor_v1;

namespace noctalia::theme {
  struct GeneratedPalette;
}

namespace settings {
  class PaletteInspectorContent;
  class SettingsDialogPresenter;
  struct SettingsContentContext;
} // namespace settings

// Standalone xdg-toplevel settings UI (same binary as the shell; shares RenderContext).
class SettingsWindow {
public:
  SettingsWindow();
  ~SettingsWindow();

  void initialize(
      WaylandConnection& wayland, ConfigService* config, RenderContext* renderContext, DependencyService* dependencies,
      UPowerService* upower, IdleManager* idleManager, CompositorPlatform* platform, AccountsService* accounts = nullptr
  );

  void open(std::string context = "");
  void openToBarWidget(std::string barName, std::string widgetName);
  void close();
  // Closes the window for a widget editor while preserving the active section for its return.
  void closeForWidgetEditor();
  // Reopens the section saved by closeForWidgetEditor(), if any.
  void reopenAfterWidgetEditor();
  // Rows that depend on what the compositor offers (e.g. blur switches) may have come or gone.
  void onCompositorCapabilitiesChanged();
  [[nodiscard]] bool isOpen() const noexcept { return m_surface != nullptr && m_surface->isRunning(); }
  [[nodiscard]] wl_surface* wlSurface() const noexcept {
    return m_surface != nullptr ? m_surface->wlSurface() : nullptr;
  }
  [[nodiscard]] bool ownsKeyboardSurface(wl_surface* surface) const noexcept;
  [[nodiscard]] std::optional<LayerPopupParentContext> popupParentContextForSurface(wl_surface* surface) const;
  [[nodiscard]] std::optional<LayerPopupParentContext> fallbackPopupParentContext() const;

  [[nodiscard]] bool onPointerEvent(const PointerEvent& event);
  void onKeyboardEvent(const KeyboardEvent& event);
  void onThemeChanged();
  void setResolvedPalette(const noctalia::theme::GeneratedPalette& generated, std::string_view mode);
  void onFontChanged();
  void requestRedraw();
  void onExternalOptionsChanged();
  void setOpenDesktopWidgetEditor(std::function<void()> callback) { m_openDesktopWidgetEditor = std::move(callback); }
  void setOpenLockscreenWidgetEditor(std::function<void()> callback) {
    m_openLockscreenWidgetEditor = std::move(callback);
  }
  void setOpenWallpaperPanel(std::function<void()> callback) { m_openWallpaperPanel = std::move(callback); }
  void setSyncGreeterAppearance(std::function<void()> callback) { m_syncGreeterAppearance = std::move(callback); }
  void setResetLauncherUsage(std::function<void()> callback) { m_resetLauncherUsage = std::move(callback); }
  void setResetScreenTime(std::function<void()> callback) { m_resetScreenTime = std::move(callback); }
  void setResetEncryptedStorage(std::function<void()> callback) { m_resetEncryptedStorage = std::move(callback); }
  void setSaveWallpaperPaletteAsCustom(std::function<void()> callback) {
    m_saveWallpaperPaletteAsCustom = std::move(callback);
  }
  void setCalendarService(CalendarService* service) { m_calendarService = service; }
  // Source for the bar widget gesture action picker.
  void setIpcService(IpcService* service) { m_ipcService = service; }
  void setClipboardService(ClipboardService* service) { m_clipboardService = service; }
  // Backs store thumbnails; trimmed when the window closes.
  void setAsyncTextureCache(AsyncTextureCache* cache) { m_asyncTextures = cache; }
  void initializeDialogPresenter(
      ColorPickerDialogPresenter& colorFallback, GlyphPickerDialogPresenter& glyphFallback,
      FileDialogPresenter& fileFallback, ThumbnailService& thumbnails
  );
  void shutdownDialogPresenter();
  [[nodiscard]] ColorPickerDialogPresenter* colorPickerDialogPresenter() noexcept;
  [[nodiscard]] GlyphPickerDialogPresenter* glyphPickerDialogPresenter() noexcept;
  [[nodiscard]] FileDialogPresenter* fileDialogPresenter() noexcept;

  void onSecondTick();
  void onIdleLiveStatusChanged();
  void markSettingsWriteSuccess(bool requestRebuild = true);
  void markSettingsWriteError(std::string message);
  void warnOnUnusableCustomSchedule(const std::vector<std::string>& path);
  void writePaw2Settings(std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides);
  void resetPaw2Settings(std::vector<std::vector<std::string>> paths);
  [[nodiscard]] bool
  applyPaw2Overrides(const std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>>& overrides);
  void applyPaw2Resets(const std::vector<std::vector<std::string>>& paths);
  void applyLinkedPaw2Settings(const std::vector<std::string>& path, const ConfigOverrideValue* value);
  void onPaw2Changed();
  [[nodiscard]] std::string paw2Notice() const;
  void showTransientStatus(std::string message, bool isError = false);

private:
  void destroyWindow();
  // While a paw2 shortcut is being recorded, paw2 passes its own shortcuts to this window
  // instead of running them, so Super+Return is recorded rather than opening a terminal.
  void setShortcutsInhibited(bool inhibited);
  [[nodiscard]] bool shouldUseModalDialogs() const noexcept;
  void prepareFrame(bool needsUpdate, bool needsLayout);
  void buildScene(std::uint32_t width, std::uint32_t height);
  void rebuildSettingsContent();
  [[nodiscard]] settings::RegistryEnvironment buildRegistryEnvironment() const;
  [[nodiscard]] std::vector<settings::SelectOption> availableOutputs() const;
  void refreshSettingsRegistry(const Config& cfg);
  void syncSelectedBarState(const Config& cfg, const std::vector<std::string>& availableBars);
  [[nodiscard]] std::unique_ptr<Flex> buildHeaderRow(float scale);
  [[nodiscard]] std::unique_ptr<Flex>
  buildFilterRow(float scale, const std::string& resetPageScope, std::vector<std::vector<std::string>> resetPagePaths);
  [[nodiscard]] std::unique_ptr<Flex> buildStatusRow(float scale);
  [[nodiscard]] std::unique_ptr<Flex> buildBody(
      float scale, const Config& cfg, const std::vector<settings::SettingsSection>& sections,
      const std::vector<std::string>& availableBars
  );
  [[nodiscard]] std::vector<settings::SelectOption> batteryDeviceOptions() const;
  // Bindable IPC commands, for the bar widget gesture action picker.
  [[nodiscard]] std::vector<settings::GestureActionOption> gestureActionCatalog() const;
  [[nodiscard]] settings::SettingsContentContext makeContentContext(
      const Config& cfg, const BarConfig* selectedBar, const BarMonitorOverride* selectedMonitorOverride
  );
  // Readies the editor sheet for a new editor: false when the window can't host one. Closes any
  // open editor and popup first, and creates the sheet on first use.
  [[nodiscard]] bool prepareEditorSheet();
  // Stores a list with a row removed by its editor, then closes the editor and rebuilds the page.
  void commitRowRemoval(std::vector<std::string> path, ConfigOverrideValue rows);
  // Content context for an editor hosted in the sheet, for the selected bar and override.
  [[nodiscard]] settings::SettingsContentContext hostedEditorContext(const Config& cfg);
  [[nodiscard]] std::vector<std::vector<std::string>> currentPageResetPaths() const;
  [[nodiscard]] bool
  tryPatchSettingsRegistryValue(const std::vector<std::string>& path, const ConfigOverrideValue& value);
  [[nodiscard]] bool tryPatchSettingsRegistryOverrides(
      const std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>>& overrides
  );
  [[nodiscard]] bool tryPatchSettingsRegistryResetValues(const std::vector<std::vector<std::string>>& paths);
  void rebuildFilterRow(float scale);
  void requestSceneRebuild();
  void
  requestContentRebuild(bool refreshRegistry = false, bool refreshFilterRow = false, bool rebuildEditorSheet = false);
  void scheduleDeferredRebuild();
  void maybeOpenPendingEditor();
  void applyPendingContentScrollTarget(float margin);
  void scrollFocusedAreaIntoView(class InputArea* area);
  void scrollSidebarNodeIntoView(const Node* node);
  void clearStatusMessage();
  void clearTransientSettingsState();
  void finishSettingsWrite(
      bool changed, bool forceSceneRebuild, bool pageResetPathsChanged, bool registryAlreadyCurrent,
      bool rebuildWhenUnchanged = false
  );
  void openActionsMenu();
  void openConfigExportDialog();
  void openBarWidgetAddPopup(const std::vector<std::string>& lanePath);
  // Request is taken by value because opening the popup can close the sheet that owns the forwarding control.
  void openSearchPickerPopup(settings::SearchPickerOpenRequest request);
  void openMonitorOverrideCreateDialog(std::optional<std::string> barName);
  void openSessionActionEntryEditor(std::size_t index);
  void syncSessionActionInlineSummary(std::size_t index, const SessionPanelActionConfig& row);
  void openIdleBehaviorEntryEditor(std::size_t index);
  void openIdleBehaviorCreateEditor();
  void openNotificationFilterEntryEditor(std::size_t index);
  void openNotificationFilterCreateEditor();
  void openCalendarAccountEditor(std::optional<std::string> accountId);
  void openWidgetInspectorEditor(std::vector<std::string> laneListPath, std::string widgetName);
  void openCapsuleGroupEditor(std::vector<std::string> laneListPath, std::string groupId);
  void openCommunityTemplateStore();
  void openPaletteInspector();
  void openBarWidgetEditorSheet(
      std::string title, std::function<void(Flex&)> populate, std::function<void()> removeAction = nullptr
  );
  void closeWidgetInspectorPopup();
  void refreshIdleLiveStatusText();
  void saveSupportReport();
  void saveConfigExport(settings::ConfigExportMode mode);
  [[nodiscard]] bool headerDragRegionContains(float sceneX, float sceneY) const;
  void setSettingOverride(std::vector<std::string> path, ConfigOverrideValue value);
  void setSettingOverrides(std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides);
  void clearSettingOverride(std::vector<std::string> path);
  void clearSettingOverrides(std::vector<std::vector<std::string>> paths);
  // Reverts a bar lane to the config file, including the capsule groups that lane holds.
  void resetBarLane(std::vector<std::string> lanePath);
  void renameWidgetInstance(
      std::string oldName, std::string newName,
      std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> referenceOverrides
  );
  void createBar(std::string name);
  void renameBar(std::string oldName, std::string newName);
  void deleteBar(std::string name);
  void moveBar(std::string name, int direction);
  void createMonitorOverride(std::string barName, std::string match);
  void renameMonitorOverride(std::string barName, std::string oldMatch, std::string newMatch);
  void deleteMonitorOverride(std::string barName, std::string match);
  void createDockMonitorOverride(std::string match);
  void renameDockMonitorOverride(std::string oldTableName, std::string newMatch);
  void deleteDockMonitorOverride(std::string tableName);
  [[nodiscard]] float uiScale() const;

  [[nodiscard]] std::optional<LayerPopupParentContext> topmostPopupParentContext() const;
  void dismissOpenSelectDropdown();

  WaylandConnection* m_wayland = nullptr;
  zwp_keyboard_shortcuts_inhibitor_v1* m_shortcutsInhibitor = nullptr;
  CompositorPlatform* m_platform = nullptr;
  Signal<>::ScopedConnection m_paw2ListenerToken;
  // paw2's config generation the open pages were built from.
  std::uint64_t m_paw2ConfigGeneration = 0;
  bool m_paw2Connected = false;
  // Connector and modes of each monitor the Displays page shows.
  std::vector<std::pair<std::string, std::vector<compositors::paw2::Paw2OutputMode>>> m_paw2Displays;
  IdleManager* m_idleManager = nullptr;
  ConfigService* m_config = nullptr;
  RenderContext* m_renderContext = nullptr;
  DependencyService* m_dependencies = nullptr;
  UPowerService* m_upower = nullptr;
  AccountsService* m_accounts = nullptr;
  CalendarService* m_calendarService = nullptr;
  ClipboardService* m_clipboardService = nullptr;
  std::unordered_map<std::string, std::uint32_t> m_templateColors;
  std::string m_templateColorMode;
  std::shared_ptr<settings::PaletteInspectorContent> m_paletteInspectorContent;
  IpcService* m_ipcService = nullptr;
  AsyncTextureCache* m_asyncTextures = nullptr;
  Label* m_idleLiveStatusLabel = nullptr;
  std::vector<Label*> m_sessionActionSummaryLabels;
  std::shared_ptr<std::vector<SessionPanelActionConfig>> m_sessionActionsEditState;

  // m_sceneRoot must be destroyed before m_animations — ~Node() calls cancelForOwner().
  AnimationManager m_animations;
  std::unique_ptr<ToplevelSurface> m_surface;
  std::unique_ptr<Node> m_sceneRoot;
  Flex* m_mainContainer = nullptr; // Outer Flex inside m_sceneRoot, sized to the window
  Box* m_panelBackground = nullptr;
  Node* m_headerRow = nullptr;
  Node* m_filterRow = nullptr;
  Button* m_actionsMenuButton = nullptr;
  Flex* m_contentContainer = nullptr;
  Flex* m_pageTitleRow = nullptr;
  Flex* m_groupJumpRow = nullptr;
  ScrollView* m_contentScrollView = nullptr;
  ScrollView* m_sidebarScrollView = nullptr;
  RovingListNavHost* m_sidebarNav = nullptr;
  std::unique_ptr<ContextMenuPopup> m_actionsMenuPopup;
  std::unique_ptr<settings::WidgetAddPopup> m_widgetAddPopup;
  std::unique_ptr<settings::ConfigExportDialogModal> m_configExportDialogModal;
  std::unique_ptr<settings::SearchPickerPopup> m_searchPickerPopup;
  InputDispatcher m_inputDispatcher;
  settings::SettingsModalHost m_modalHost;
  std::unique_ptr<settings::SettingsDialogPresenter> m_dialogPresenter;
  std::unique_ptr<settings::SettingsSheetModal> m_editorSheetModal;
  std::unique_ptr<settings::SettingsControlFactory> m_editorSheetFactory;
  std::vector<std::string> m_editorSheetListPath;
  std::unique_ptr<SelectDropdownPopup> m_selectPopup;
  bool m_pointerInside = false;
  wl_output* m_output = nullptr;
  std::uint32_t m_minWidthHint = 0;
  std::uint32_t m_minHeightHint = 0;

  std::uint32_t m_lastSceneWidth = 0;
  std::uint32_t m_lastSceneHeight = 0;
  ScrollViewState m_sidebarScrollState;
  ScrollViewState m_contentScrollState;
  std::vector<settings::SettingEntry> m_settingsRegistry;
  bool m_rebuildRequested = false;
  bool m_contentRebuildRequested = false;
  bool m_settingsRegistryRefreshRequested = false;
  bool m_filterRowRefreshRequested = false;
  bool m_deferredRebuildQueued = false;
  bool m_deferredSceneRebuild = false;
  bool m_deferredRefreshRegistry = false;
  bool m_deferredRefreshFilterRow = false;
  bool m_deferredRebuildEditorSheet = false;
  bool m_focusSearchOnRebuild = false;
  Input* m_settingsSearchInput = nullptr;
  bool m_scrollToPendingContentTarget = false;
  // While restoring focus after a content rebuild, defer scroll-into-view until the scene is laid
  // out; applying it against un-positioned nodes would collapse the scroll offset to the top.
  bool m_deferFocusScrollToLayout = false;
  Node* m_pendingContentScrollTarget = nullptr;
  std::string m_searchQuery;
  Timer m_searchDebounceTimer;
  // Set by openToBarWidget (e.g. middle-click on a bar widget) and consumed after
  // the Settings scene is available so the requested editor can be mounted into it.
  std::string m_pendingOpenWidgetInspectorName;
  std::string m_editingWidgetName;
  std::string m_editingCapsuleGroupId;
  std::vector<std::string> m_selectedLaneWidgets;
  std::string m_pendingDeleteWidgetName;
  std::string m_pendingDeleteWidgetSettingPath;
  std::string m_renamingWidgetName;
  // Gesture whose action row has a chosen command that still needs its argument typed.
  std::string m_pendingGestureKey;
  std::string m_pendingGestureVerb;
  // The widget whose actions group is unfolded, empty when none. Keyed by widget rather than a
  // plain flag so the group survives the rebuild an edit triggers, but starts folded on every
  // other widget.
  std::string m_actionsExpandedFor;
  // Expanded setting groups per page, keyed by content section key (pageScopeKey).
  // A page gets its default expansion (first group, or all with settings_expand_all_groups) when first
  // rendered; changing settings_expand_all_groups drops this state so every page re-seeds.
  std::unordered_map<std::string, std::unordered_set<std::string>> m_expandedSettingGroups;
  bool m_expandedSettingGroupsSeededExpandAll = false;
  std::string m_creatingBarName;
  std::string m_renamingBarName;
  std::string m_pendingDeleteBarName;
  std::string m_renamingMonitorOverrideBarName;
  std::string m_renamingMonitorOverrideMatch;
  std::string m_pendingDeleteMonitorOverrideBarName;
  std::string m_pendingDeleteMonitorOverrideMatch;
  std::string m_renamingDockMonitorOverride;
  std::string m_pendingDeleteDockMonitorOverride;
  std::string m_selectedBarName;
  std::string m_selectedMonitorOverride;
  std::string m_selectedSection;
  std::string m_reopenAfterWidgetEditorSection;
  std::string m_statusMessage;
  std::string m_pendingResetPageScope;
  std::vector<std::vector<std::string>> m_pendingResetSettingPaths;
  bool m_forceEnTranslation = false;
  bool m_showAdvanced = false;
  bool m_showOverriddenOnly = false;
  bool m_statusIsError = false;
  bool m_pendingEncryptedStorageReset = false;
  std::function<void()> m_openDesktopWidgetEditor;
  std::function<void()> m_openLockscreenWidgetEditor;
  std::function<void()> m_openWallpaperPanel;
  std::function<void()> m_syncGreeterAppearance;
  std::function<void()> m_resetLauncherUsage;
  std::function<void()> m_resetScreenTime;
  std::function<void()> m_resetEncryptedStorage;
  std::function<void()> m_saveWallpaperPaletteAsCustom;
};
