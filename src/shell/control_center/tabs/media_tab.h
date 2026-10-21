#pragma once

#include "core/timer_manager.h"
#include "dbus/mpris/mpris_service.h"
#include "shell/control_center/tab.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class Button;
class ContextMenuPopup;
class HttpClient;
class Image;
class Label;
class MprisService;
class PipeWireSpectrum;
class RenderContext;
class Slider;
class AudioVisualizer;
class ConfigService;
class WaylandConnection;

class MediaTab : public Tab {
public:
  MediaTab(
      MprisService* mpris, HttpClient* httpClient, PipeWireSpectrum* spectrum, ConfigService* config,
      WaylandConnection* wayland, RenderContext* renderContext
  );
  ~MediaTab() override;

  std::unique_ptr<Flex> create() override;
  void onFrameTick(float deltaMs) override;
  void setActive(bool active) override;
  void onClose() override;
  bool dismissTransientUi() override;

private:
  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;
  void refresh(Renderer& renderer);
  void clearArt(Renderer& renderer);
  void commitPendingSeek(double valueSeconds);

  void openPlayerMenu();

  // Guard token for deferred callbacks that run on the next main-loop tick.
  // Callbacks capture a weak_ptr so they can detect destruction without
  // relying on a raw this pointer staying valid.
  std::shared_ptr<void> m_aliveGuard = std::make_shared<int>(0);

  MprisService* m_mpris = nullptr;
  HttpClient* m_httpClient = nullptr;
  PipeWireSpectrum* m_spectrum = nullptr;
  ConfigService* m_config = nullptr;
  WaylandConnection* m_wayland = nullptr;
  RenderContext* m_renderContext = nullptr;
  std::uint64_t m_spectrumListenerId = 0;
  bool m_active = false;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* rootLayout = nullptr;
    Flex* visualizerBody = nullptr;
    AudioVisualizer* visualizerSpectrum = nullptr;
    Image* artwork = nullptr;
    Flex* artworkRow = nullptr;
    Flex* nowCard = nullptr;
    Flex* mediaStack = nullptr;
    Button* playerMenuButton = nullptr;
    Label* trackTitle = nullptr;
    Label* trackArtist = nullptr;
    Label* trackAlbum = nullptr;
    Slider* progressSlider = nullptr;
    Button* prevButton = nullptr;
    Button* playPauseButton = nullptr;
    Button* nextButton = nullptr;
    Button* repeatButton = nullptr;
    Button* shuffleButton = nullptr;
  };
  Ui m_ui;
  std::unique_ptr<ContextMenuPopup> m_playerMenuPopup;

  std::string m_lastArtPath;
  std::string m_lastBusName;
  std::string m_lastPlaybackStatus;
  std::string m_lastLoopStatus;
  bool m_lastShuffle = false;
  bool m_syncingProgress = false;
  std::int64_t m_pendingSeekUs = -1;
  std::string m_pendingSeekBusName;
  std::chrono::steady_clock::time_point m_pendingSeekUntil;
  std::chrono::steady_clock::time_point m_progressSettleUntil;
  bool m_playerMenuOpen = false;
  std::vector<std::string> m_playerBusNames;
  std::unordered_set<std::string> m_pendingArtDownloads;
  std::string m_positionBusName;
  std::string m_positionTrackId;
  std::string m_positionTrackSignature;
  std::int64_t m_positionUs = 0;
  std::int64_t m_lastTrackLengthUs = 0;
  std::chrono::steady_clock::time_point m_positionSampleAt;
  std::optional<MprisPlayerInfo> m_lastActiveSnapshot;
  std::chrono::steady_clock::time_point m_lastActiveSeenAt;
  Timer m_progressTimer;
};
