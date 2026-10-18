#pragma once

#include "compositors/workspace_backend.h"

#include <cstddef>
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
class Paw2WorkspaceBackend final : public compositors::WorkspaceMetadataBackend {
public:
  explicit Paw2WorkspaceBackend(compositors::paw2::Paw2Runtime& runtime);
  ~Paw2WorkspaceBackend() override;

  Paw2WorkspaceBackend(const Paw2WorkspaceBackend&) = delete;
  Paw2WorkspaceBackend& operator=(const Paw2WorkspaceBackend&) = delete;

  void setChangeCallback(ChangeCallback callback) override;
  [[nodiscard]] int pollFd() const noexcept override;
  [[nodiscard]] short pollEvents() const noexcept override;
  [[nodiscard]] int pollTimeoutMs() const noexcept override;
  void dispatchPoll(short revents) override;
  void apply(std::vector<Workspace>& workspaces, const std::string& outputName = {}) const override;
  [[nodiscard]] std::vector<std::string> workspaceKeys(const std::string& outputName = {}) const override;
  [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>>
  appIdsByWorkspace(const std::string& outputName = {}) const override;
  [[nodiscard]] std::vector<WorkspaceWindow> workspaceWindows(const std::string& outputName = {}) const override;
  // Exact only once paw2 trusts us: an untrusted peer gets blank identifiers (and no toplevel list).
  [[nodiscard]] bool hasExactWindowIdentity() const noexcept override;
  [[nodiscard]] std::optional<std::string> focusedWindowId() const override;
  bool focusWindowById(const std::string& windowId, bool warpPointer = false) override;
  bool closeWindowById(const std::string& windowId) override;
  void cleanup() override;

private:
  compositors::paw2::Paw2Runtime& m_runtime;
  std::size_t m_listenerToken = 0;
  ChangeCallback m_changeCallback;
};
