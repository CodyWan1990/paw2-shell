#pragma once

#include "compositors/workspace_backend.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct wl_output;
struct ext_workspace_manager_v1;

class ExtWorkspaceBackend;

// Workspace list from ext-workspace-v1, which paw2 implements natively. Occupancy and the
// window-to-workspace map are layered on top by the paw2 metadata backend in CompositorPlatform.
class WaylandWorkspaces {
public:
  using ChangeCallback = std::function<void()>;

  WaylandWorkspaces();
  ~WaylandWorkspaces();

  void bindExtWorkspace(ext_workspace_manager_v1* manager);
  void initialize();
  void setChangeCallback(ChangeCallback callback);
  void activate(const std::string& id);
  void activateForOutput(wl_output* output, const std::string& id);
  void activateForOutput(wl_output* output, const Workspace& workspace);
  void cleanup();
  [[nodiscard]] int pollFd() const noexcept;
  [[nodiscard]] short pollEvents() const noexcept;
  [[nodiscard]] int pollTimeoutMs() const noexcept;
  void dispatchPoll(short revents);
  [[nodiscard]] const char* backendName() const noexcept;
  [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>> appIdsByWorkspace(wl_output* output) const;
  [[nodiscard]] TaskbarAssignmentMode taskbarAssignmentMode() const noexcept;
  [[nodiscard]] std::unordered_map<std::uintptr_t, WorkspaceWindow>
  assignTaskbarWindows(const std::vector<TaskbarWindowCandidate>& windows, wl_output* output) const;
  [[nodiscard]] std::vector<WorkspaceWindow> workspaceWindows(wl_output* output) const;
  [[nodiscard]] std::vector<std::string> openOverlayWorkspaceKeys(wl_output* output) const;
  void focusWindow(const std::string& windowId) const;

  [[nodiscard]] std::vector<Workspace> all() const;
  [[nodiscard]] std::vector<Workspace> forOutput(wl_output* output) const;

private:
  [[nodiscard]] const WorkspaceBackend* active() const noexcept;
  [[nodiscard]] WorkspaceBackend* active() noexcept;

  std::unique_ptr<ExtWorkspaceBackend> m_ext;
  bool m_active = false;
  ChangeCallback m_changeCallback;
};
