#include "compositors/compositor_platform.h"

#include "compositors/compositor_detect.h"
#include "compositors/compositor_runtime.h"
#include "compositors/ext_workspace/ext_workspace_output_backend.h"
#include "compositors/paw2/paw2_runtime.h"
#include "compositors/paw2/paw2_workspace_backend.h"
#include "compositors/workspace_alert_service.h"
#include "core/log.h"
#include "core/process/process.h"
#include "util/string_utils.h"
#include "wayland/output_probe.h"
#include "wayland/wayland_connection.h"
#include "wayland/wayland_workspaces.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace compositors {

  class FocusedOutputBackend {
  public:
    virtual ~FocusedOutputBackend() = default;
    [[nodiscard]] virtual std::optional<std::string> focusedOutputName() const = 0;
    [[nodiscard]] virtual std::unordered_map<std::string, bool> outputPowerStates() const = 0;
    virtual void setOutputPowerChangeCallback(CompositorPlatform::ChangeCallback callback) = 0;
  };

  class OutputPowerBackend {
  public:
    virtual ~OutputPowerBackend() = default;
    [[nodiscard]] virtual bool setOutputPower(WaylandConnection& wayland, bool on) const = 0;
    [[nodiscard]] virtual bool isPerOutputTargeted() const noexcept { return false; }
  };

} // namespace compositors

namespace {

  constexpr Logger kLog("compositor_platform");

  [[nodiscard]] std::unordered_set<std::string>
  windowIdsFromAssignments(const std::vector<WorkspaceWindowAssignment>& assignments) {
    std::unordered_set<std::string> windowIds;
    windowIds.reserve(assignments.size());
    for (const auto& assignment : assignments) {
      if (!assignment.windowId.empty()) {
        windowIds.insert(assignment.windowId);
      }
    }
    return windowIds;
  }

  void
  retainToplevelsWithWindowIds(std::vector<ToplevelInfo>& windows, const std::unordered_set<std::string>& windowIds) {
    std::erase_if(windows, [&](const ToplevelInfo& window) { return !windowIds.contains(window.identifier); });
  }

  [[nodiscard]] const char* valueOrUnset(const char* value) {
    return value != nullptr && value[0] != '\0' ? value : "<unset>";
  }

  void logSessionExitContext(compositors::CompositorKind compositor) {
    kLog.info(
        "logout requested: compositor={} env_hint=\"{}\" xdg_session_id={} user={}", compositors::name(compositor),
        compositors::envHint(), valueOrUnset(std::getenv("XDG_SESSION_ID")), valueOrUnset(std::getenv("USER"))
    );
  }

  [[nodiscard]] bool requestLoginSessionExit() {
    if (const char* sessionId = std::getenv("XDG_SESSION_ID"); sessionId != nullptr && sessionId[0] != '\0') {
      if (process::launchFirstAvailable({{"loginctl", "terminate-session", sessionId}})) {
        return true;
      }
    }
    if (process::launchFirstAvailable({{"systemctl", "--user", "stop", "graphical-session.target"}})) {
      return true;
    }
    if (const char* user = std::getenv("USER"); user != nullptr && user[0] != '\0') {
      if (process::launchFirstAvailable({{"loginctl", "terminate-user", user}})) {
        return true;
      }
    }
    return false;
  }

  // Focus and output-power state from the paw2 control socket.
  class Paw2FocusedOutputBackend final : public compositors::FocusedOutputBackend {
  public:
    explicit Paw2FocusedOutputBackend(const compositors::paw2::Paw2Runtime& runtime) : m_runtime(runtime) {}

    [[nodiscard]] std::optional<std::string> focusedOutputName() const override {
      if (const auto* output = m_runtime.focusedOutput(); output != nullptr) {
        return output->name;
      }
      return std::nullopt;
    }
    [[nodiscard]] std::unordered_map<std::string, bool> outputPowerStates() const override { return {}; }
    void setOutputPowerChangeCallback(CompositorPlatform::ChangeCallback /*callback*/) override {}

  private:
    const compositors::paw2::Paw2Runtime& m_runtime;
  };

  class LambdaOutputPowerBackend final : public compositors::OutputPowerBackend {
  public:
    using Callback = std::function<bool(WaylandConnection&, bool)>;

    explicit LambdaOutputPowerBackend(Callback callback, bool perOutputTargeted = false)
        : m_callback(std::move(callback)), m_perOutputTargeted(perOutputTargeted) {}

    [[nodiscard]] bool setOutputPower(WaylandConnection& wayland, bool on) const override {
      return m_callback && m_callback(wayland, on);
    }
    [[nodiscard]] bool isPerOutputTargeted() const noexcept override { return m_perOutputTargeted; }

  private:
    Callback m_callback;
    bool m_perOutputTargeted = false;
  };

  [[nodiscard]] bool setGenericOutputPower(WaylandConnection& /*wayland*/, bool on) {
    return compositors::ext_workspace::setOutputPower(on);
  }

  [[nodiscard]] std::unique_ptr<compositors::OutputPowerBackend> createOutputPowerBackend() {
    return std::make_unique<LambdaOutputPowerBackend>(&setGenericOutputPower);
  }

} // namespace

CompositorPlatform::CompositorPlatform(WaylandConnection& wayland)
    : m_wayland(wayland), m_runtimeRegistry(std::make_unique<compositors::CompositorRuntimeRegistry>()),
      m_workspaces(std::make_unique<WaylandWorkspaces>()) {
  if (compositors::isPaw2()) {
    auto& paw2 = m_runtimeRegistry->paw2();
    m_workspaceMetadataBackend = std::make_unique<Paw2WorkspaceBackend>(paw2);
    m_focusedOutputBackends.push_back(std::make_unique<Paw2FocusedOutputBackend>(paw2));
    m_paw2ListenerToken = paw2.addListener([this]() { onPaw2Changed(); });
  }
  m_outputPowerBackend = createOutputPowerBackend();

  m_wayland.setWorkspaceManagerCallback([this](ext_workspace_manager_v1* manager) { bindExtWorkspace(manager); });
  m_wayland.setToplevelChangeCallback([this]() { notifyToplevelsChanged(); });
  m_wayland.setOutputLifecycleCallbacks([this](wl_output* output) { onOutputAdded(output); }, {});
}

CompositorPlatform::~CompositorPlatform() {
  cleanup();
  m_paw2ListenerToken.disconnect();
  m_wayland.setOutputLifecycleCallbacks({}, {});
  m_wayland.setWorkspaceManagerCallback({});
  m_wayland.setToplevelChangeCallback({});
}

void CompositorPlatform::initialize() {
  if (m_initialized) {
    return;
  }
  m_initialized = true;

  m_workspaces->initialize();
}

void CompositorPlatform::cleanup() {
  if (m_workspaceMetadataBackend != nullptr) {
    m_workspaceMetadataBackend->cleanup();
  }
  if (m_workspaces != nullptr) {
    m_workspaces->cleanup();
  }
  m_initialized = false;
}

wl_display* CompositorPlatform::display() const noexcept { return m_wayland.display(); }

compositors::paw2::Paw2Runtime& CompositorPlatform::paw2Runtime() noexcept { return m_runtimeRegistry->paw2(); }

const compositors::paw2::Paw2Runtime& CompositorPlatform::paw2Runtime() const noexcept {
  return m_runtimeRegistry->paw2();
}

compositors::paw2::Paw2ConfigWriter& CompositorPlatform::paw2ConfigWriter() noexcept {
  return m_runtimeRegistry->paw2Config();
}

const compositors::paw2::Paw2ConfigWriter& CompositorPlatform::paw2ConfigWriter() const noexcept {
  return m_runtimeRegistry->paw2Config();
}

bool CompositorPlatform::hasXdgShell() const noexcept { return m_wayland.hasXdgShell(); }

bool CompositorPlatform::hasXdgActivation() const noexcept { return m_wayland.hasXdgActivation(); }

std::string CompositorPlatform::requestActivationToken(wl_surface* surface) const {
  return m_wayland.requestActivationToken(surface);
}

bool CompositorPlatform::hasGammaControl() const noexcept { return m_wayland.hasGammaControl(); }

const std::vector<WaylandOutput>& CompositorPlatform::outputs() const noexcept { return m_wayland.outputs(); }

const WaylandOutput* CompositorPlatform::findOutputByWl(wl_output* output) const {
  return m_wayland.findOutputByWl(output);
}

wl_output* CompositorPlatform::outputForSurface(wl_surface* surface) const noexcept {
  return m_wayland.outputForSurface(surface);
}


bool CompositorPlatform::hasPointerPosition() const noexcept { return m_wayland.hasPointerPosition(); }

wl_surface* CompositorPlatform::lastPointerSurface() const noexcept { return m_wayland.lastPointerSurface(); }

wl_surface* CompositorPlatform::lastKeyboardSurface() const noexcept { return m_wayland.lastKeyboardSurface(); }

double CompositorPlatform::lastPointerX() const noexcept { return m_wayland.lastPointerX(); }

double CompositorPlatform::lastPointerY() const noexcept { return m_wayland.lastPointerY(); }

std::uint32_t CompositorPlatform::lastInputSerial() const noexcept { return m_wayland.lastInputSerial(); }

zwlr_layer_surface_v1* CompositorPlatform::layerSurfaceFor(wl_surface* surface) const noexcept {
  return m_wayland.layerSurfaceFor(surface);
}

void CompositorPlatform::stopKeyRepeat() { m_wayland.stopKeyRepeat(); }

void CompositorPlatform::setCursorShape(std::uint32_t serial, std::uint32_t shape) {
  m_wayland.setCursorShape(serial, shape);
}

wl_output* CompositorPlatform::focusedInteractiveOutput(std::chrono::milliseconds pointerMaxAge) const {
  const auto outputReady = [this](wl_output* output) {
    const auto* info = m_wayland.findOutputByWl(output);
    return info != nullptr && info->done && info->output != nullptr && info->hasUsableGeometry();
  };

  for (const auto& backend : m_focusedOutputBackends) {
    if (backend == nullptr) {
      continue;
    }
    if (const auto focusedName = backend->focusedOutputName(); focusedName.has_value()) {
      if (wl_output* output = resolveOutputName(*focusedName); output != nullptr) {
        if (outputReady(output)) {
          return output;
        }
      }
    }
  }

  if (wl_output* output = m_wayland.activeToplevelOutput(); output != nullptr) {
    if (outputReady(output)) {
      return output;
    }
  }

  if (wl_surface* keyboardSurface = m_wayland.lastKeyboardSurface(); keyboardSurface != nullptr) {
    if (wl_output* output = m_wayland.outputForSurface(keyboardSurface); output != nullptr) {
      if (outputReady(output)) {
        return output;
      }
    }
  }

  if (m_wayland.hasFreshPointerOutput(pointerMaxAge)) {
    if (wl_output* output = m_wayland.lastPointerOutput(); outputReady(output)) {
      return output;
    }
  }

  return nullptr;
}

wl_output* CompositorPlatform::preferredInteractiveOutput(std::chrono::milliseconds pointerMaxAge) const {
  if (wl_output* focused = focusedInteractiveOutput(pointerMaxAge); focused != nullptr) {
    return focused;
  }

  const auto& outputs = m_wayland.outputs();
  const auto it = std::ranges::find_if(outputs, [](const WaylandOutput& output) {
    return output.done && output.output != nullptr && output.hasUsableGeometry();
  });
  return it != outputs.end() ? it->output : nullptr;
}

void CompositorPlatform::probeFocusedOutput(
    std::function<void(wl_output*)> callback, std::chrono::milliseconds timeout
) {
  // Replace any in-flight probe; only the latest request matters. A finished
  // probe goes inert (surface torn down, timer stopped) and lingers here until
  // the next probe or destruction replaces it.
  m_outputProbe = std::make_unique<OutputProbe>(m_wayland, timeout, std::move(callback));
}

std::optional<ActiveToplevel> CompositorPlatform::activeToplevel() const {
  if (const auto& paw2 = m_runtimeRegistry->paw2(); paw2.hasSnapshot()) {
    const auto* window = paw2.focusedWindow();
    if (window == nullptr) {
      return std::nullopt;
    }
    return ActiveToplevel{
        .title = window->title,
        .appId = window->appId,
        .identifier = window->identifier,
        .handle = nullptr,
    };
  }
  return m_wayland.activeToplevel();
}

wl_output* CompositorPlatform::activeToplevelOutput() const {
  if (const auto& paw2 = m_runtimeRegistry->paw2(); paw2.hasSnapshot()) {
    if (const auto* window = paw2.focusedWindow(); window != nullptr) {
      if (wl_output* output = resolveOutputName(window->output); output != nullptr) {
        return output;
      }
    }
  }
  return m_wayland.activeToplevelOutput();
}

std::vector<std::string> CompositorPlatform::runningAppIds(wl_output* outputFilter) const {
  auto ids = m_wayland.runningAppIds(outputFilter);
  // ext-foreign-toplevel-list does not say which output a window is on, so the Wayland side ignores the filter.
  // When the compositor reports window identities with their outputs, keep only the apps with a window there.
  if (outputFilter != nullptr
      && m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->hasExactWindowIdentity()
      && m_wayland.hasExtForeignToplevelList()) {
    std::erase_if(ids, [&](const std::string& id) {
      return enrichedWindowsForApp(StringUtils::toLower(id), {}, outputFilter).empty();
    });
  }
  return ids;
}

std::vector<ToplevelInfo> CompositorPlatform::windowsForApp(
    const std::string& idLower, const std::string& wmClassLower, wl_output* outputFilter
) const {
  return m_wayland.windowsForApp(idLower, wmClassLower, outputFilter);
}

std::vector<ToplevelInfo> CompositorPlatform::windowsWithoutAppId(wl_output* outputFilter) const {
  return m_wayland.windowsWithoutAppId(outputFilter);
}

std::vector<ToplevelInfo> CompositorPlatform::enrichedWindowsForApp(
    const std::string& idLower, const std::string& wmClassLower, wl_output* outputFilter
) const {
  if (m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->hasExactWindowIdentity()
      && m_wayland.hasExtForeignToplevelList()) {
    auto windows = m_wayland.extWindowsForApp(idLower, wmClassLower);
    for (auto& w : windows) {
      w.exactIdentity = true;
    }
    if (!windows.empty()) {
      retainToplevelsWithWindowIds(windows, windowIdsFromAssignments(workspaceWindowAssignments(outputFilter)));
    }
    // Do not fall back to title/app-id matching while one side of the exact
    // identity join is still pending. The ext `done` or IPC update will retry.
    return windows;
  }
  return windowsForApp(idLower, wmClassLower, outputFilter);
}

std::vector<ToplevelInfo> CompositorPlatform::enrichedWindowsWithoutAppId(wl_output* outputFilter) const {
  if (m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->hasExactWindowIdentity()
      && m_wayland.hasExtForeignToplevelList()) {
    auto windows = m_wayland.extWindowsWithoutAppId();
    for (auto& w : windows) {
      w.exactIdentity = true;
    }
    if (!windows.empty()) {
      retainToplevelsWithWindowIds(windows, windowIdsFromAssignments(workspaceWindowAssignments(outputFilter)));
    }
    return windows;
  }
  return windowsWithoutAppId(outputFilter);
}

bool CompositorPlatform::hasExactWindowIdentity() const noexcept {
  return m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->hasExactWindowIdentity()
      && m_wayland.hasExtForeignToplevelList();
}

void CompositorPlatform::activateToplevel(zwlr_foreign_toplevel_handle_v1* handle) {
  m_wayland.activateToplevel(handle);
}

void CompositorPlatform::activateToplevelForAppId(std::string_view appId) {
  if (appId.empty()) {
    return;
  }
  const std::string idLower = StringUtils::toLower(std::string(appId));
  const auto windows = windowsForApp(idLower, idLower);
  if (windows.empty()) {
    return;
  }
  activateToplevelInfo(windows.back());
}

void CompositorPlatform::activateToplevelInfo(const ToplevelInfo& window) {
  if (window.exactIdentity
      && !window.identifier.empty()
      && m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->focusWindowById(window.identifier)) {
    return;
  }
  if (window.handle != nullptr) {
    activateToplevel(window.handle);
    return;
  }
  if (!window.identifier.empty()) {
    focusCompositorWindow(window.identifier);
  }
}

void CompositorPlatform::closeToplevel(zwlr_foreign_toplevel_handle_v1* handle) { m_wayland.closeToplevel(handle); }

void CompositorPlatform::closeToplevelInfo(const ToplevelInfo& window) {
  if (window.handle != nullptr) {
    closeToplevel(window.handle);
    return;
  }
  if (window.exactIdentity && !window.identifier.empty() && m_workspaceMetadataBackend != nullptr) {
    (void)m_workspaceMetadataBackend->closeWindowById(window.identifier);
  }
}

void CompositorPlatform::minimizeToplevelInfo(const ToplevelInfo& window) {
  if (window.exactIdentity && !window.identifier.empty() && m_workspaceMetadataBackend != nullptr) {
    (void)m_workspaceMetadataBackend->minimizeWindowById(window.identifier);
  }
}

std::vector<std::string> CompositorPlatform::minimizedCompositorWindowIds() const {
  if (m_workspaceMetadataBackend == nullptr) {
    return {};
  }
  return m_workspaceMetadataBackend->minimizedWindowIds();
}

bool CompositorPlatform::containsWlrToplevelHandle(zwlr_foreign_toplevel_handle_v1* handle) const {
  return m_wayland.containsWlrToplevelHandle(handle);
}

void CompositorPlatform::setToplevelChangeCallback(ChangeCallback callback) {
  m_toplevelChangeCallback = std::move(callback);
}

void CompositorPlatform::notifyToplevelsChanged() {
  if (m_toplevelChangeCallback) {
    m_toplevelChangeCallback();
  }
}

std::optional<std::string> CompositorPlatform::focusedCompositorWindowId() const {
  if (m_workspaceMetadataBackend != nullptr) {
    if (auto id = m_workspaceMetadataBackend->focusedWindowId(); id.has_value() && !id->empty()) {
      return id;
    }
  }
  return std::nullopt;
}

void CompositorPlatform::setWorkspaceChangeCallback(ChangeCallback callback) {
  m_workspaceChangeCallback = std::move(callback);
  m_lastWorkspaceModelSnapshot = workspaceModelSnapshot();
  m_lastFocusedCompositorWindowId = focusedCompositorWindowId();
  m_lastMinimizedCompositorWindowIds = minimizedCompositorWindowIds();
  auto wrapper = [this]() {
    auto nextSnapshot = workspaceModelSnapshot();
    auto nextFocused = focusedCompositorWindowId();
    auto nextMinimized = minimizedCompositorWindowIds();
    const bool minimizedChanged = nextMinimized != m_lastMinimizedCompositorWindowIds;
    if (sameWorkspaceModelSnapshot(nextSnapshot, m_lastWorkspaceModelSnapshot)
        && nextFocused == m_lastFocusedCompositorWindowId && !minimizedChanged) {
      return;
    }
    m_lastWorkspaceModelSnapshot = std::move(nextSnapshot);
    m_lastFocusedCompositorWindowId = std::move(nextFocused);
    m_lastMinimizedCompositorWindowIds = std::move(nextMinimized);
    if (m_workspaceChangeCallback) {
      m_workspaceChangeCallback();
    }
    // A window minimizing changes how the taskbar and dock draw it, not where it is.
    if (minimizedChanged && m_toplevelChangeCallback) {
      m_toplevelChangeCallback();
    }
  };

  if (m_workspaces != nullptr) {
    m_workspaces->setChangeCallback(wrapper);
  }
  if (m_workspaceMetadataBackend != nullptr) {
    m_workspaceMetadataBackend->setChangeCallback(wrapper);
  }
}

void CompositorPlatform::setOverviewChangeCallback(ChangeCallback callback) {
  m_overviewChangeCallback = std::move(callback);
  if (m_workspaceMetadataBackend != nullptr) {
    m_workspaceMetadataBackend->setOverviewChangeCallback(m_overviewChangeCallback);
  }
}

void CompositorPlatform::activateWorkspace(const std::string& id) {
  if (m_workspaces != nullptr) {
    m_workspaces->activate(id);
  }
}

void CompositorPlatform::activateWorkspace(wl_output* output, const std::string& id) {
  if (m_workspaces != nullptr) {
    m_workspaces->activateForOutput(output, id);
  }
}

void CompositorPlatform::activateWorkspace(wl_output* output, const Workspace& workspace) {
  if (m_workspaces != nullptr) {
    m_workspaces->activateForOutput(output, workspace);
  }
}

std::size_t CompositorPlatform::addWorkspacePollFds(std::vector<pollfd>& fds) const {
  const auto start = fds.size();
  if (m_workspaces != nullptr && m_workspaces->pollFd() >= 0) {
    fds.push_back({.fd = m_workspaces->pollFd(), .events = m_workspaces->pollEvents(), .revents = 0});
  }
  if (m_workspaceMetadataBackend != nullptr && m_workspaceMetadataBackend->pollFd() >= 0) {
    fds.push_back(
        {.fd = m_workspaceMetadataBackend->pollFd(), .events = m_workspaceMetadataBackend->pollEvents(), .revents = 0}
    );
  }
  return start;
}

int CompositorPlatform::workspacePollTimeoutMs() const noexcept {
  int timeout = m_workspaces != nullptr ? m_workspaces->pollTimeoutMs() : -1;
  if (m_workspaceMetadataBackend != nullptr) {
    const int trackerTimeout = m_workspaceMetadataBackend->pollTimeoutMs();
    if (trackerTimeout >= 0 && (timeout < 0 || trackerTimeout < timeout)) {
      timeout = trackerTimeout;
    }
  }
  return timeout;
}

void CompositorPlatform::dispatchWorkspacePoll(const std::vector<pollfd>& fds, std::size_t startIdx) {
  std::size_t index = startIdx;
  if (m_workspaces != nullptr
      && m_workspaces->pollFd() >= 0
      && index < fds.size()
      && fds[index].fd == m_workspaces->pollFd()) {
    m_workspaces->dispatchPoll(fds[index].revents);
    ++index;
  }

  if (m_workspaceMetadataBackend != nullptr) {
    short revents = 0;
    if (m_workspaceMetadataBackend->pollFd() >= 0
        && index < fds.size()
        && fds[index].fd == m_workspaceMetadataBackend->pollFd()) {
      revents = fds[index].revents;
    }
    m_workspaceMetadataBackend->dispatchPoll(revents);
  }
}

std::vector<Workspace> CompositorPlatform::workspaces() const {
  auto current = m_workspaces != nullptr ? m_workspaces->all() : std::vector<Workspace>{};
  if (m_workspaceMetadataBackend != nullptr) {
    m_workspaceMetadataBackend->apply(current);
  }
  if (m_workspaceAlertService != nullptr) {
    m_workspaceAlertService->applyOverlay(current);
  }
  return current;
}

std::vector<Workspace> CompositorPlatform::workspaces(wl_output* output) const {
  auto current = m_workspaces != nullptr ? m_workspaces->forOutput(output) : std::vector<Workspace>{};
  if (m_workspaceMetadataBackend != nullptr) {
    m_workspaceMetadataBackend->apply(current, connectorNameForOutput(output));
  }
  if (m_workspaceAlertService != nullptr) {
    m_workspaceAlertService->applyOverlay(current);
  }
  return current;
}

void CompositorPlatform::setWorkspaceAlertService(WorkspaceAlertService* service) noexcept {
  m_workspaceAlertService = service;
}

bool CompositorPlatform::isKnownWorkspaceAlertKey(std::string_view workspaceId) const {
  if (workspaceId.empty()) {
    return false;
  }
  const auto& outputs = m_wayland.outputs();
  if (outputs.empty()) {
    return WorkspaceAlertService::isKnownWorkspaceToken(workspaceId, workspaces());
  }
  for (const auto& output : outputs) {
    if (WorkspaceAlertService::isKnownWorkspaceToken(workspaceId, workspaces(output.output))) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> CompositorPlatform::workspaceAlertKeyForWindow(std::string_view windowId) const {
  if (windowId.empty()) {
    return std::nullopt;
  }
  // The assignment key is the backend's own workspace handle; the alert overlay
  // matches it against a workspace's id/name/index, so store it directly once
  // it resolves to a current workspace.
  const auto resolveForOutput = [this, windowId](wl_output* output) -> std::optional<std::string> {
    const auto token = WorkspaceAlertService::workspaceTokenForWindow(windowId, workspaceWindowAssignments(output));
    if (token.has_value() && WorkspaceAlertService::isKnownWorkspaceToken(*token, workspaces(output))) {
      return token;
    }
    return std::nullopt;
  };

  const auto& outputs = m_wayland.outputs();
  if (outputs.empty()) {
    return resolveForOutput(nullptr);
  }
  for (const auto& output : outputs) {
    if (auto token = resolveForOutput(output.output); token.has_value()) {
      return token;
    }
  }
  return std::nullopt;
}

std::size_t CompositorPlatform::clearActiveWorkspaceAlerts(wl_output* output) {
  if (m_workspaceAlertService == nullptr || m_workspaceAlertService->empty()) {
    return 0;
  }
  return m_workspaceAlertService->clearActive(workspaces(output));
}

std::size_t CompositorPlatform::clearActiveWorkspaceAlerts() {
  if (m_workspaceAlertService == nullptr || m_workspaceAlertService->empty()) {
    return 0;
  }
  std::size_t cleared = 0;
  const auto& outputs = m_wayland.outputs();
  if (outputs.empty()) {
    return m_workspaceAlertService->clearActive(workspaces());
  }
  for (const auto& output : outputs) {
    cleared += m_workspaceAlertService->clearActive(workspaces(output.output));
  }
  return cleared;
}

std::unordered_map<std::string, std::vector<std::string>>
CompositorPlatform::appIdsByWorkspace(wl_output* outputFilter) const {
  if (m_workspaceMetadataBackend != nullptr) {
    const auto fromMetadata = m_workspaceMetadataBackend->appIdsByWorkspace(connectorNameForOutput(outputFilter));
    if (!fromMetadata.empty()) {
      return fromMetadata;
    }
  }
  return m_workspaces != nullptr ? m_workspaces->appIdsByWorkspace(outputFilter)
                                 : std::unordered_map<std::string, std::vector<std::string>>{};
}

std::vector<std::string> CompositorPlatform::workspaceDisplayKeys(wl_output* outputFilter) const {
  if (m_workspaceMetadataBackend != nullptr) {
    return m_workspaceMetadataBackend->workspaceKeys(connectorNameForOutput(outputFilter));
  }
  return {};
}

std::vector<WorkspaceWindowAssignment> CompositorPlatform::workspaceWindowAssignments(wl_output* outputFilter) const {
  std::vector<WorkspaceWindow> windows;
  if (m_workspaceMetadataBackend != nullptr) {
    windows = m_workspaceMetadataBackend->workspaceWindows(connectorNameForOutput(outputFilter));
  }
  if (windows.empty() && m_workspaces != nullptr) {
    windows = m_workspaces->workspaceWindows(outputFilter);
  }

  std::vector<WorkspaceWindowAssignment> result;
  result.reserve(windows.size());
  for (const auto& window : windows) {
    result.push_back(
        WorkspaceWindowAssignment{
            .windowId = window.windowId,
            .workspaceKey = window.workspaceKey,
            .appId = window.appId,
            .title = window.title,
            .x = window.x,
            .y = window.y,
        }
    );
  }
  return result;
}

std::vector<std::string> CompositorPlatform::openOverlayWorkspaceKeys(wl_output* outputFilter) const {
  return m_workspaces != nullptr ? m_workspaces->openOverlayWorkspaceKeys(outputFilter) : std::vector<std::string>{};
}

TaskbarAssignmentMode CompositorPlatform::taskbarAssignmentMode() const noexcept {
  return m_workspaces != nullptr ? m_workspaces->taskbarAssignmentMode() : TaskbarAssignmentMode::Generic;
}

bool CompositorPlatform::supportsTaskbarWorkspaceGrouping() const noexcept {
  if (taskbarAssignmentMode() == TaskbarAssignmentMode::WorkspaceOccurrenceTitle) {
    return true;
  }

  if (m_workspaces != nullptr && std::string_view(m_workspaces->backendName()) != "none") {
    return true;
  }
  if (m_workspaceMetadataBackend != nullptr && !m_workspaceMetadataBackend->workspaceKeys({}).empty()) {
    return true;
  }
  return false;
}

std::unordered_map<std::uintptr_t, WorkspaceWindow> CompositorPlatform::assignTaskbarWindows(
    const std::vector<TaskbarWindowCandidate>& windows, wl_output* outputFilter
) const {
  return m_workspaces != nullptr ? m_workspaces->assignTaskbarWindows(windows, outputFilter)
                                 : std::unordered_map<std::uintptr_t, WorkspaceWindow>{};
}

const char* CompositorPlatform::workspaceBackendName() const noexcept {
  return m_workspaces != nullptr ? m_workspaces->backendName() : "none";
}

bool CompositorPlatform::closeCompositorWindow(const std::string& windowId) const {
  return !windowId.empty()
      && m_workspaceMetadataBackend != nullptr
      && m_workspaceMetadataBackend->closeWindowById(windowId);
}

void CompositorPlatform::focusCompositorWindow(const std::string& windowId, bool warpPointer) const {
  if (m_workspaceMetadataBackend != nullptr && m_workspaceMetadataBackend->focusWindowById(windowId, warpPointer)) {
    return;
  }
  if (m_workspaces != nullptr) {
    m_workspaces->focusWindow(windowId);
  }
}

bool CompositorPlatform::cycleKeyboardLayout() const {
  return m_keyboardLayoutBackend != nullptr && m_keyboardLayoutBackend->cycleLayout();
}

bool CompositorPlatform::hasKeyboardLayoutBackend() const noexcept {
  return m_keyboardLayoutBackend != nullptr && m_keyboardLayoutBackend->isAvailable();
}

std::optional<KeyboardLayoutState> CompositorPlatform::keyboardLayoutState() const {
  if (m_keyboardLayoutBackend == nullptr) {
    return std::nullopt;
  }
  return m_keyboardLayoutBackend->layoutState();
}

void CompositorPlatform::onPaw2Changed() {
  // The socket is also the only source of the focused window (paw2 has no
  // wlr-foreign-toplevel-management), so a snapshot is a toplevel change too.
  notifyToplevelsChanged();

  const auto& keyboard = m_runtimeRegistry->paw2().snapshot().keyboard;
  if (!keyboard.has_value()) {
    return;
  }
  // The first report counts as a change too: it is what the layout OSD takes as its baseline.
  if (m_lastPaw2KeyboardLayout != keyboard->layout) {
    m_lastPaw2KeyboardLayout = keyboard->layout;
    if (m_keyboardLayoutChangeCallback) {
      m_keyboardLayoutChangeCallback();
    }
  }
  const WaylandSeat::LockKeysState lockKeys{
      .capsLock = keyboard->capsLock, .numLock = keyboard->numLock, .scrollLock = keyboard->scrollLock
  };
  if (m_lastPaw2LockKeys != lockKeys) {
    m_lastPaw2LockKeys = lockKeys;
    if (m_lockKeysChangeCallback) {
      m_lockKeysChangeCallback();
    }
  }
}

std::optional<WaylandSeat::LockKeysState> CompositorPlatform::compositorLockKeysState() const {
  return m_lastPaw2LockKeys;
}

void CompositorPlatform::setLockKeysChangeCallback(ChangeCallback callback) {
  m_lockKeysChangeCallback = std::move(callback);
}

std::string CompositorPlatform::currentKeyboardLayoutName() const {
  if (m_lastPaw2KeyboardLayout.has_value() && !m_lastPaw2KeyboardLayout->empty()) {
    return *m_lastPaw2KeyboardLayout;
  }
  if (m_keyboardLayoutBackend != nullptr) {
    if (auto backendName = m_keyboardLayoutBackend->currentLayoutName();
        backendName.has_value() && !backendName->empty()) {
      return *backendName;
    }
  }
  return m_wayland.currentKeyboardLayoutName();
}

std::vector<std::string> CompositorPlatform::keyboardLayoutNames() const {
  if (const auto& keyboard = m_runtimeRegistry->paw2().snapshot().keyboard;
      m_lastPaw2KeyboardLayout.has_value() && keyboard.has_value() && !keyboard->layouts.empty()) {
    return keyboard->layouts;
  }
  if (m_keyboardLayoutBackend != nullptr) {
    if (const auto state = m_keyboardLayoutBackend->layoutState(); state.has_value() && !state->names.empty()) {
      if (state->names.size() > 1) {
        return state->names;
      }
      auto waylandNames = m_wayland.keyboardLayoutNames();
      return waylandNames.size() > state->names.size() ? waylandNames : state->names;
    }
  }
  return m_wayland.keyboardLayoutNames();
}

void CompositorPlatform::setKeyboardLayoutChangeCallback(ChangeCallback callback) {
  m_keyboardLayoutChangeCallback = std::move(callback);
  if (m_keyboardLayoutBackend != nullptr) {
    m_keyboardLayoutBackend->setChangeCallback(m_keyboardLayoutChangeCallback);
    m_keyboardLayoutBackend->connectSocket();
  }
}

void CompositorPlatform::addKeyboardLayoutPollFds(std::vector<pollfd>& fds) const {
  if (m_keyboardLayoutBackend != nullptr && m_keyboardLayoutBackend->pollFd() >= 0) {
    fds.push_back(
        {.fd = m_keyboardLayoutBackend->pollFd(), .events = m_keyboardLayoutBackend->pollEvents(), .revents = 0}
    );
  }
}

void CompositorPlatform::dispatchKeyboardLayoutPoll(const std::vector<pollfd>& fds, std::size_t startIdx) {
  if (m_keyboardLayoutBackend != nullptr && m_keyboardLayoutBackend->pollFd() >= 0 && startIdx < fds.size()) {
    m_keyboardLayoutBackend->dispatchPoll(fds[startIdx].revents);
  }
}

bool CompositorPlatform::requestSessionExit() const {
  const compositors::CompositorKind compositor = compositors::detect();
  logSessionExitContext(compositor);

  // paw2 only honours quit from a trusted peer and sends no reply, so a refusal
  // is indistinguishable from success here. A dev build running from a user path
  // is untrusted: that case logs out through logind instead.
  if (compositor == compositors::CompositorKind::Paw2 && m_runtimeRegistry->paw2().trusted()) {
    if (m_runtimeRegistry->paw2().quit()) {
      return true;
    }
  }
  return requestLoginSessionExit();
}

bool CompositorPlatform::setOutputPower(bool on) const {
  if (m_outputPowerBackend == nullptr) {
    return false;
  }
  // A failed multi-output request may still have powered off some outputs.
  if (!on) {
    m_lastRequestedOutputPowerState = false;
  }
  if (!m_outputPowerBackend->setOutputPower(m_wayland, on)) {
    return false;
  }
  m_lastRequestedOutputPowerState = on;
  return true;
}

bool CompositorPlatform::shouldSuppressLockTransition() const {
  if (m_outputPowerOffPending) {
    return true;
  }
  std::unordered_map<std::string, bool> states;
  for (const auto& backend : m_focusedOutputBackends) {
    states = backend->outputPowerStates();
  }
  bool hasOutput = false;
  bool allKnown = true;
  for (const auto& output : m_wayland.outputs()) {
    if (!output.done || output.output == nullptr || !output.hasUsableGeometry()) {
      continue;
    }
    hasOutput = true;
    const auto state = states.find(output.connectorName);
    if (state == states.end()) {
      allKnown = false;
    } else if (!state->second) {
      return true;
    }
  }
  if (hasOutput && allKnown) {
    return false;
  }
  return m_lastRequestedOutputPowerState == false;
}

void CompositorPlatform::setOutputPowerChangeCallback(ChangeCallback callback) {
  for (auto& backend : m_focusedOutputBackends) {
    backend->setOutputPowerChangeCallback(callback);
  }
}

bool CompositorPlatform::tracksOverviewState() const noexcept {
  return m_workspaceMetadataBackend != nullptr && m_workspaceMetadataBackend->canTrackOverviewState();
}

bool CompositorPlatform::hasOverviewState() const noexcept {
  return m_workspaceMetadataBackend != nullptr && m_workspaceMetadataBackend->hasOverviewState();
}

bool CompositorPlatform::isOverviewOpen() const noexcept {
  if (m_workspaceMetadataBackend == nullptr || !m_workspaceMetadataBackend->hasOverviewState()) {
    return true;
  }
  return m_workspaceMetadataBackend->isOverviewOpen();
}

void CompositorPlatform::bindExtWorkspace(ext_workspace_manager_v1* manager) {
  if (m_workspaces != nullptr) {
    m_workspaces->bindExtWorkspace(manager);
  }
}

void CompositorPlatform::onOutputAdded(wl_output* /*output*/) {
  if (m_outputPowerBackend != nullptr
      && m_outputPowerBackend->isPerOutputTargeted()
      && m_lastRequestedOutputPowerState.has_value()) {
    (void)m_outputPowerBackend->setOutputPower(m_wayland, *m_lastRequestedOutputPowerState);
  }
}

wl_output* CompositorPlatform::resolveOutputName(const std::string& outputName) const {
  if (outputName.empty()) {
    return nullptr;
  }
  for (const auto& output : m_wayland.outputs()) {
    if (output.output == nullptr) {
      continue;
    }
    if (output.connectorName == outputName || output.description == outputName) {
      return output.output;
    }
  }
  return nullptr;
}

std::string CompositorPlatform::connectorNameForOutput(wl_output* output) const {
  if (output == nullptr) {
    return {};
  }
  for (const auto& candidate : m_wayland.outputs()) {
    if (candidate.output == output) {
      return candidate.connectorName;
    }
  }
  return {};
}

std::vector<CompositorPlatform::WorkspaceModelSnapshot> CompositorPlatform::workspaceModelSnapshot() const {
  auto sortedAssignments = [](std::vector<WorkspaceWindowAssignment> assignments) {
    std::ranges::sort(assignments, [](const auto& lhs, const auto& rhs) {
      if (lhs.windowId != rhs.windowId) {
        return lhs.windowId < rhs.windowId;
      }
      if (lhs.workspaceKey != rhs.workspaceKey) {
        return lhs.workspaceKey < rhs.workspaceKey;
      }
      return lhs.appId < rhs.appId;
    });
    return assignments;
  };

  auto makeSnapshot = [&](const WaylandOutput* output) {
    auto* wlOutput = output != nullptr ? output->output : nullptr;
    return WorkspaceModelSnapshot{
        .outputName = output != nullptr ? output->name : 0,
        .workspaces = workspaces(wlOutput),
        .assignments = sortedAssignments(workspaceWindowAssignments(wlOutput)),
    };
  };

  std::vector<WorkspaceModelSnapshot> snapshot;
  const auto& outputs = m_wayland.outputs();
  if (outputs.empty()) {
    snapshot.push_back(makeSnapshot(nullptr));
    return snapshot;
  }

  snapshot.reserve(outputs.size());
  for (const auto& output : outputs) {
    snapshot.push_back(makeSnapshot(&output));
  }
  return snapshot;
}

bool CompositorPlatform::sameWorkspaceModelSnapshot(
    const std::vector<WorkspaceModelSnapshot>& lhs, const std::vector<WorkspaceModelSnapshot>& rhs
) {
  auto sameWorkspace = [](const Workspace& a, const Workspace& b) {
    return a.id == b.id
        && a.name == b.name
        && a.coordinates == b.coordinates
        && a.active == b.active
        && a.urgent == b.urgent
        && a.occupied == b.occupied;
  };
  auto sameAssignment = [](const WorkspaceWindowAssignment& a, const WorkspaceWindowAssignment& b) {
    return a.windowId == b.windowId
        && a.workspaceKey == b.workspaceKey
        && a.appId == b.appId
        && a.title == b.title
        && a.x == b.x
        && a.y == b.y;
  };

  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].outputName != rhs[i].outputName
        || lhs[i].workspaces.size() != rhs[i].workspaces.size()
        || lhs[i].assignments.size() != rhs[i].assignments.size()) {
      return false;
    }
    for (std::size_t w = 0; w < lhs[i].workspaces.size(); ++w) {
      if (!sameWorkspace(lhs[i].workspaces[w], rhs[i].workspaces[w])) {
        return false;
      }
    }
    for (std::size_t a = 0; a < lhs[i].assignments.size(); ++a) {
      if (!sameAssignment(lhs[i].assignments[a], rhs[i].assignments[a])) {
        return false;
      }
    }
  }
  return true;
}
