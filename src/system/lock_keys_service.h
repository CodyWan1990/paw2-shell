#pragma once

#include "wayland/wayland_seat.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

class WaylandConnection;

class LockKeysService {
public:
  using LockKeysState = WaylandSeat::LockKeysState;
  using ChangeCallback = std::function<void(const LockKeysState& previous, const LockKeysState& current)>;
  // The compositor's own report of the lock keys, when it has one. While it answers, sysfs is
  // not polled; the compositor's change notification should call refreshNow().
  using CompositorSource = std::function<std::optional<LockKeysState>()>;

  explicit LockKeysService(WaylandConnection& wayland);

  [[nodiscard]] LockKeysState state() const noexcept;
  void setChangeCallback(ChangeCallback callback);
  void setCompositorSource(CompositorSource source);
  [[nodiscard]] int pollTimeoutMs() const;
  void dispatchPoll();
  void refreshNow();

private:
  [[nodiscard]] LockKeysState readCurrentState();
  [[nodiscard]] std::optional<LockKeysState> readCachedSysfsState();
  void discoverSysfsLeds();
  [[nodiscard]] bool hasCachedSysfsLeds() const noexcept;

  WaylandConnection& m_wayland;
  LockKeysState m_state;
  bool m_hasState = false;
  bool m_sysfsDiscovered = false;
  int m_sysfsReadFailures = 0;
  std::chrono::steady_clock::time_point m_nextRefreshAt;
  ChangeCallback m_changeCallback;
  CompositorSource m_compositorSource;
  std::vector<std::filesystem::path> m_capsLockPaths;
  std::vector<std::filesystem::path> m_numLockPaths;
  std::vector<std::filesystem::path> m_scrollLockPaths;
};
