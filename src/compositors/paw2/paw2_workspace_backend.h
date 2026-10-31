#pragma once

#include "compositors/workspace_backend.h"
#include "ui/signal.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace compositors::paw2 {
  class Paw2Runtime;
} // namespace compositors::paw2

// Workspace metadata from paw2's control socket, joined to the ext protocol lists the platform
// already owns. Workspace order and names come from ext-workspace; occupancy and the
// window-to-workspace map come from the socket snapshot. paw2 publishes each window's
// ext-foreign-toplevel identifier, so window ids here are that identifier and the join is exact.
class Paw2WorkspaceBackend final {
public:
  using ChangeCallback = std::function<void()>;

  explicit Paw2WorkspaceBackend(compositors::paw2::Paw2Runtime& runtime);
  ~Paw2WorkspaceBackend();

  Paw2WorkspaceBackend(const Paw2WorkspaceBackend&) = delete;
  Paw2WorkspaceBackend& operator=(const Paw2WorkspaceBackend&) = delete;

  void setChangeCallback(ChangeCallback callback);
  [[nodiscard]] int pollFd() const noexcept;
  [[nodiscard]] short pollEvents() const noexcept;
  [[nodiscard]] int pollTimeoutMs() const noexcept;
  void dispatchPoll(short revents);
  void apply(std::vector<Workspace>& workspaces, const std::string& outputName = {}) const;
  [[nodiscard]] std::vector<std::string> workspaceKeys(const std::string& outputName = {}) const;
  [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>>
  appIdsByWorkspace(const std::string& outputName = {}) const;
  [[nodiscard]] std::vector<WorkspaceWindow> workspaceWindows(const std::string& outputName = {}) const;
  // Exact only once paw2 trusts us: an untrusted peer gets blank identifiers (and no toplevel list).
  [[nodiscard]] bool hasExactWindowIdentity() const noexcept;
  [[nodiscard]] std::optional<std::string> focusedWindowId() const;
  bool focusWindowById(const std::string& windowId, bool warpPointer = false);
  bool closeWindowById(const std::string& windowId);
  bool minimizeWindowById(const std::string& windowId);
  [[nodiscard]] std::vector<std::string> minimizedWindowIds() const;
  void cleanup();

private:
  compositors::paw2::Paw2Runtime& m_runtime;
  Signal<>::ScopedConnection m_listenerToken;
  ChangeCallback m_changeCallback;
};
