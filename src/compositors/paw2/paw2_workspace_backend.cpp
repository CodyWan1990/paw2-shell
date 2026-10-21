#include "compositors/paw2/paw2_workspace_backend.h"

#include "compositors/paw2/paw2_runtime.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

using compositors::paw2::Paw2Runtime;
using compositors::paw2::workspaceIdFor;

namespace {

  // Display and assignment key: the workspace's ext-workspace name, which paw2 sets to the
  // 1-based index. Keys are only unique per output, which is how the taskbar queries them.
  [[nodiscard]] std::string workspaceKeyFor(std::uint32_t zeroBasedIndex) { return std::to_string(zeroBasedIndex + 1); }

} // namespace

Paw2WorkspaceBackend::Paw2WorkspaceBackend(Paw2Runtime& runtime) : m_runtime(runtime) {
  m_listenerToken = m_runtime.addListener([this]() {
    if (m_changeCallback) {
      m_changeCallback();
    }
  });
}

Paw2WorkspaceBackend::~Paw2WorkspaceBackend() = default;

void Paw2WorkspaceBackend::setChangeCallback(ChangeCallback callback) { m_changeCallback = std::move(callback); }

int Paw2WorkspaceBackend::pollFd() const noexcept { return m_runtime.pollFd(); }

short Paw2WorkspaceBackend::pollEvents() const noexcept { return m_runtime.pollEvents(); }

int Paw2WorkspaceBackend::pollTimeoutMs() const noexcept { return m_runtime.pollTimeoutMs(); }

void Paw2WorkspaceBackend::dispatchPoll(short revents) { m_runtime.dispatchPoll(revents); }

void Paw2WorkspaceBackend::apply(std::vector<Workspace>& workspaces, const std::string& outputName) const {
  if (!m_runtime.hasSnapshot()) {
    return;
  }
  // ext-workspace already reports active; occupancy is socket-only.
  std::unordered_set<std::string> occupied;
  for (const auto& output : m_runtime.snapshot().outputs) {
    if (!outputName.empty() && output.name != outputName) {
      continue;
    }
    for (std::uint32_t i = 0; i < output.occupied.size(); ++i) {
      if (output.occupied[i]) {
        occupied.insert(workspaceIdFor(output.name, i));
      }
    }
  }
  for (auto& workspace : workspaces) {
    workspace.occupied = occupied.contains(workspace.id);
  }
}

std::vector<std::string> Paw2WorkspaceBackend::workspaceKeys(const std::string& outputName) const {
  std::vector<std::string> keys;
  for (const auto& output : m_runtime.snapshot().outputs) {
    if (!outputName.empty() && output.name != outputName) {
      continue;
    }
    for (std::uint32_t i = 0; i < output.occupied.size(); ++i) {
      keys.push_back(workspaceKeyFor(i));
    }
  }
  return keys;
}

std::unordered_map<std::string, std::vector<std::string>>
Paw2WorkspaceBackend::appIdsByWorkspace(const std::string& outputName) const {
  std::unordered_map<std::string, std::vector<std::string>> result;
  std::unordered_map<std::string, std::unordered_set<std::string>> seen;
  for (const auto& window : workspaceWindows(outputName)) {
    if (window.appId.empty()) {
      continue;
    }
    if (seen[window.workspaceKey].insert(window.appId).second) {
      result[window.workspaceKey].push_back(window.appId);
    }
  }
  return result;
}

std::vector<WorkspaceWindow> Paw2WorkspaceBackend::workspaceWindows(const std::string& outputName) const {
  std::vector<WorkspaceWindow> result;
  for (const auto& window : m_runtime.snapshot().windows) {
    if (window.identifier.empty()) {
      continue;
    }
    if (!outputName.empty() && window.output != outputName) {
      continue;
    }
    result.push_back(
        WorkspaceWindow{
            .windowId = window.identifier,
            .workspaceKey = workspaceKeyFor(window.workspace),
            .appId = window.appId,
            .title = window.title,
            .x = 0,
            .y = 0,
            .outputName = window.output,
        }
    );
  }
  return result;
}

bool Paw2WorkspaceBackend::hasExactWindowIdentity() const noexcept {
  return std::ranges::any_of(m_runtime.snapshot().windows, [](const auto& w) { return !w.identifier.empty(); });
}

std::optional<std::string> Paw2WorkspaceBackend::focusedWindowId() const {
  const auto* window = m_runtime.focusedWindow();
  if (window == nullptr || window->identifier.empty()) {
    return std::nullopt;
  }
  return window->identifier;
}

bool Paw2WorkspaceBackend::focusWindowById(const std::string& windowId, bool /*warpPointer*/) {
  const auto* window = m_runtime.findWindowByIdentifier(windowId);
  return window != nullptr && m_runtime.focusWindow(window->id);
}

bool Paw2WorkspaceBackend::closeWindowById(const std::string& windowId) {
  const auto* window = m_runtime.findWindowByIdentifier(windowId);
  return window != nullptr && m_runtime.closeWindow(window->id);
}

bool Paw2WorkspaceBackend::minimizeWindowById(const std::string& windowId) {
  const auto* window = m_runtime.findWindowByIdentifier(windowId);
  return window != nullptr && m_runtime.minimizeWindow(window->id);
}

std::vector<std::string> Paw2WorkspaceBackend::minimizedWindowIds() const {
  std::vector<std::string> ids;
  for (const auto& window : m_runtime.snapshot().windows) {
    if (window.minimized && !window.identifier.empty()) {
      ids.push_back(window.identifier);
    }
  }
  return ids;
}

void Paw2WorkspaceBackend::cleanup() { m_runtime.cleanup(); }
