#pragma once

#include "core/timer_manager.h"
#include "shell/panel/panel.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Button;
class AsyncTextureCache;
class ClipboardService;
class Flex;
class Image;
class Input;
class InputArea;
class Label;
class Renderer;
class ScrollView;
class ConfigService;
class ClipboardListAdapter;
class VirtualGridView;
struct ClipboardEntry;

class ClipboardPanel : public Panel {
public:
  ClipboardPanel(ClipboardService* clipboard, ConfigService* config, AsyncTextureCache* asyncTextures);
  ~ClipboardPanel() override;
  void setActivateCallback(std::function<void(const ClipboardEntry&)> callback);

  void create() override;
  void onOpen(std::string_view context) override;
  void onClose() override;

  [[nodiscard]] float preferredWidth() const override { return scaled(720.0F); }
  [[nodiscard]] float preferredHeight() const override { return scaled(560.0F); }
  [[nodiscard]] LayerShellKeyboard keyboardMode() const override { return LayerShellKeyboard::Exclusive; }
  [[nodiscard]] bool handleGlobalKey(std::uint32_t sym, std::uint32_t modifiers, bool pressed, bool preedit) override;
  [[nodiscard]] InputArea* initialFocusArea() const override;
  [[nodiscard]] PanelPlacement panelPlacement() const noexcept override;

private:
  void doLayout(Renderer& renderer, float width, float height) override;
  void doUpdate(Renderer& renderer) override;
  void onPanelCardOpacityChanged(float opacity) override;
  void schedulePreviewPayloadRefresh(bool debounced);
  void updateListState();
  void updatePreviewActions();
  void rebuildPreview(Renderer& renderer, float width, float height);
  void selectIndex(std::size_t index);
  void selectByStorageId(std::string storageId);
  void activateSelected();
  void togglePinSelected();
  void runImageAction();
  void requestClearUnpinnedHistory();
  void clearUnpinnedHistory();
  void clearAllHistory();
  void performClearUnpinnedHistory();
  void performClearAllHistory();
  bool handleKeyEvent(std::uint32_t sym, std::uint32_t modifiers);
  void scrollToSelected();
  void requestDeleteSelectedEntry();
  void deleteSelectedEntry();
  void performDeleteSelectedEntry();
  void resetDeleteConfirmation();
  void resetClearConfirmation();
  void applyFilter();
  void onFilterChanged(const std::string& text);
  [[nodiscard]] std::size_t selectedHistoryIndex() const;

  ClipboardService* m_clipboard = nullptr;
  std::function<void(const ClipboardEntry&)> m_activateCallback;
  ConfigService* m_config = nullptr;
  AsyncTextureCache* m_asyncTextures = nullptr;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    InputArea* focusArea = nullptr;
    Flex* rootLayout = nullptr;
    Flex* sidebar = nullptr;
    Button* clearHistoryButton = nullptr;
    Button* clearKeepPinnedButton = nullptr;
    Flex* clearConfirmPanel = nullptr;
    Label* clearConfirmDesc = nullptr;
    Button* closeButton = nullptr;
    Input* filterInput = nullptr;
    VirtualGridView* listGrid = nullptr;
    Label* listEmptyLabel = nullptr;
    Flex* previewCard = nullptr;
    Label* previewTitle = nullptr;
    Label* previewMeta = nullptr;
    Button* imageActionButton = nullptr;
    Button* pinButton = nullptr;
    Button* copyButton = nullptr;
    Button* deleteEntryButton = nullptr;
    Flex* deleteConfirmPanel = nullptr;
    ScrollView* previewScrollView = nullptr;
    Flex* previewContent = nullptr;
    Image* previewImage = nullptr;
  };
  Ui m_ui;
  std::unique_ptr<ClipboardListAdapter> m_listAdapter;
  std::vector<std::size_t> m_filteredIndices;
  std::string m_filterQuery;

  std::size_t m_selectedIndex = 0;
  std::size_t m_previewPayloadIndex = static_cast<std::size_t>(-1);
  std::size_t m_pendingPreviewPayloadIndex = static_cast<std::size_t>(-1);
  Timer m_previewPayloadDebounceTimer;
  Timer m_filterDebounceTimer;
  std::string m_pendingFilterQuery;
  std::string m_deleteConfirmStorageId;
  std::uint64_t m_lastChangeSerial = 0;
  bool m_clearConfirm = false;
  float m_lastWidth = 0.0F;
  float m_lastHeight = 0.0F;
  float m_lastPreviewWidth = -1.0F;
  float m_lastPreviewHeight = -1.0F;
  float m_listRowHeight = 0.0F;
  bool m_pendingScrollToSelected = false;
};
