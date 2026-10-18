#include "wayland/wayland_workspaces.h"

#include "compositors/ext_workspace/ext_workspace_backend.h"
#include "core/log.h"

#include <string>

namespace {

  constexpr Logger kLog("workspace");

} // namespace

WaylandWorkspaces::WaylandWorkspaces() : m_ext(std::make_unique<ExtWorkspaceBackend>()) {}

WaylandWorkspaces::~WaylandWorkspaces() = default;

void WaylandWorkspaces::bindExtWorkspace(ext_workspace_manager_v1* manager) { m_ext->bindExtWorkspace(manager); }

void WaylandWorkspaces::initialize() {
  m_active = m_ext->isAvailable();
  kLog.info("workspace backend={}", backendName());
}

void WaylandWorkspaces::setChangeCallback(ChangeCallback callback) {
  m_changeCallback = std::move(callback);
  m_ext->setChangeCallback([this]() {
    // ext-workspace can bind after initialize(); the first event makes it live.
    m_active = m_active || m_ext->isAvailable();
    if (m_changeCallback) {
      m_changeCallback();
    }
  });
}

const WorkspaceBackend* WaylandWorkspaces::active() const noexcept { return m_active ? m_ext.get() : nullptr; }

WorkspaceBackend* WaylandWorkspaces::active() noexcept { return m_active ? m_ext.get() : nullptr; }

void WaylandWorkspaces::activate(const std::string& id) {
  if (auto* backend = active()) {
    backend->activate(id);
  }
}

void WaylandWorkspaces::activateForOutput(wl_output* output, const std::string& id) {
  if (auto* backend = active()) {
    backend->activateForOutput(output, id);
  }
}

void WaylandWorkspaces::activateForOutput(wl_output* output, const Workspace& workspace) {
  if (auto* backend = active()) {
    backend->activateForOutput(output, workspace);
  }
}

void WaylandWorkspaces::cleanup() {
  m_ext->cleanup();
  m_active = false;
}

int WaylandWorkspaces::pollFd() const noexcept { return active() != nullptr ? active()->pollFd() : -1; }

short WaylandWorkspaces::pollEvents() const noexcept {
  return active() != nullptr ? active()->pollEvents() : static_cast<short>(POLLIN);
}

int WaylandWorkspaces::pollTimeoutMs() const noexcept { return active() != nullptr ? active()->pollTimeoutMs() : -1; }

void WaylandWorkspaces::dispatchPoll(short revents) {
  if (auto* backend = active()) {
    backend->dispatchPoll(revents);
  }
}

const char* WaylandWorkspaces::backendName() const noexcept {
  return active() != nullptr ? active()->backendName() : "none";
}

std::unordered_map<std::string, std::vector<std::string>>
WaylandWorkspaces::appIdsByWorkspace(wl_output* output) const {
  return active() != nullptr ? active()->appIdsByWorkspace(output)
                             : std::unordered_map<std::string, std::vector<std::string>>{};
}

TaskbarAssignmentMode WaylandWorkspaces::taskbarAssignmentMode() const noexcept {
  return active() != nullptr ? active()->taskbarAssignmentMode() : TaskbarAssignmentMode::Generic;
}

std::unordered_map<std::uintptr_t, WorkspaceWindow>
WaylandWorkspaces::assignTaskbarWindows(const std::vector<TaskbarWindowCandidate>& windows, wl_output* output) const {
  return active() != nullptr ? active()->assignTaskbarWindows(windows, output)
                             : std::unordered_map<std::uintptr_t, WorkspaceWindow>{};
}

std::vector<WorkspaceWindow> WaylandWorkspaces::workspaceWindows(wl_output* output) const {
  return active() != nullptr ? active()->workspaceWindows(output) : std::vector<WorkspaceWindow>{};
}

std::vector<std::string> WaylandWorkspaces::openOverlayWorkspaceKeys(wl_output* output) const {
  return active() != nullptr ? active()->openOverlayWorkspaceKeys(output) : std::vector<std::string>{};
}

void WaylandWorkspaces::focusWindow(const std::string& windowId) const {
  if (const auto* backend = active()) {
    const_cast<WorkspaceBackend*>(backend)->focusWindow(windowId);
  }
}

std::vector<Workspace> WaylandWorkspaces::all() const {
  return active() != nullptr ? active()->all() : std::vector<Workspace>{};
}

std::vector<Workspace> WaylandWorkspaces::forOutput(wl_output* output) const {
  return active() != nullptr ? active()->forOutput(output) : std::vector<Workspace>{};
}
