#pragma once

#include "core/timer_manager.h"
#include "shell/control_center/tab.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class ConfigService;
class Button;
class ContextMenuPopup;
class EasyEffectsService;
class Flex;
class Label;
class MprisService;
class Node;
class PipeWireService;
class RenderContext;
class Renderer;
class ScrollView;
class Select;
class Slider;
class WaylandConnection;
struct AudioNode;
struct AudioState;

class AudioTab : public Tab {
public:
  AudioTab(
      PipeWireService* audio, EasyEffectsService* easyEffects, MprisService* mpris, ConfigService* config,
      WaylandConnection* wayland, RenderContext* renderContext
  );
  ~AudioTab() override;

  std::unique_ptr<Flex> create() override;
  void onClose() override;
  bool dismissTransientUi() override;
  [[nodiscard]] bool dragging() const noexcept;

private:
  struct DeviceVolumeCardState {
    Flex* menuAnchor = nullptr;
    Button* menuButton = nullptr;
    Label* deviceLabel = nullptr;
    Slider* slider = nullptr;
    Label* valueLabel = nullptr;
    Button* muteButton = nullptr;
    Flex* effectsProfileRow = nullptr;
    Select* effectsProfileSelect = nullptr;
    Timer volumeDebounceTimer;
    bool syncing = false;
    // Slider value waiting to be sent, throttled while the slider is dragged.
    std::uint32_t pendingId = 0;
    float pendingVolume = -1.0F;
    float lastSentVolume = -1.0F;
    std::chrono::steady_clock::time_point lastCommitAt;
  };

  struct DeviceMenuModel {
    std::function<std::span<const AudioNode>(const AudioState&)> devices;
    std::function<std::uint32_t(const AudioState&)> defaultDeviceId;
    std::function<void(std::uint32_t)> activate;
  };

  struct DeviceVolumeCardSpec {
    DeviceVolumeCardState& state;
    DeviceMenuModel deviceMenu;
    std::string_view devicePrefixKey;
    std::string_view noDeviceKey;
    std::string_view muteGlyph;
    std::function<void(float)> queueVolume;
    std::function<void()> toggleMute;
  };

  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;
  std::unique_ptr<Flex> createDeviceVolumeCard(DeviceVolumeCardSpec card);
  void rebuildLists(Renderer& renderer);
  void rebuildProgramVolumes(Renderer& renderer);
  void syncValueLabelWidths(Renderer& renderer);
  void syncProgramVolumeRows();
  void queueProgramSinkVolume(std::uint32_t id, float value);
  void flushPendingProgramVolumes(bool force = false);
  [[nodiscard]] float sliderMaxPercent() const;
  void flushPendingVolumes(bool force = false);
  void flushDeviceVolume(DeviceVolumeCardState& card, bool input, bool force);

  void openDeviceMenu(DeviceVolumeCardState& card, const DeviceMenuModel& menu);
  void openProgramRoutingMenu(Node* anchor, std::uint32_t programStreamId);
  void syncEffectsProfileControls(Renderer& renderer);

  PipeWireService* m_audio = nullptr;
  EasyEffectsService* m_easyEffects = nullptr;
  MprisService* m_mpris = nullptr;
  ConfigService* m_config = nullptr;
  WaylandConnection* m_wayland = nullptr;
  RenderContext* m_renderContext = nullptr;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* rootLayout = nullptr;
    ScrollView* outputScroll = nullptr;
    ScrollView* inputScroll = nullptr;
    Flex* outputList = nullptr;
    Flex* inputList = nullptr;
    Flex* programList = nullptr;
    DeviceVolumeCardState* openDeviceMenuCard = nullptr;
  };
  Ui m_ui;
  std::vector<Flex*> m_programRows;
  std::string m_lastProgramListKey;
  float m_lastProgramSliderMax = -1.0F;
  std::string m_lastEffectsProfileListKey;
  float m_syncedPercentLabelMinWidth = -1.0F;
  float m_lastSyncedPercentLabelSliderMax = -1.0F;
  DeviceVolumeCardState m_outputDeviceVolume;
  DeviceVolumeCardState m_inputDeviceVolume;
  std::unique_ptr<ContextMenuPopup> m_deviceMenuPopup;
  std::uint32_t m_openProgramRoutingMenuStreamId = 0;

  float m_lastOutputWidth = -1.0F;
  float m_lastInputWidth = -1.0F;
  std::string m_lastOutputListKey;
  std::string m_lastInputListKey;
  float m_lastSinkVolume = -1.0F;
  float m_lastSourceVolume = -1.0F;
  Timer m_programSinkDebounceTimer;
  std::uint32_t m_pendingProgramSinkId = 0;
  float m_pendingProgramSinkVolume = -1.0F;
};
