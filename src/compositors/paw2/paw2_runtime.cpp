#include "compositors/paw2/paw2_runtime.h"

#include "core/log.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace compositors::paw2 {

  namespace {

    constexpr Logger kLog("paw2_runtime");
    constexpr auto kReconnectInitial = std::chrono::seconds(1);
    constexpr auto kReconnectMax = std::chrono::seconds(30);
    constexpr std::size_t kReadBufferMaxBytes = 4U * 1024U * 1024U;

    [[nodiscard]] std::string jsonString(const nlohmann::json& json, std::string_view key) {
      const auto it = json.find(key);
      if (it == json.end() || !it->is_string()) {
        return {};
      }
      return it->get<std::string>();
    }

    [[nodiscard]] std::uint32_t jsonIndex(const nlohmann::json& json, std::string_view key) {
      const auto it = json.find(key);
      if (it == json.end() || !it->is_number_unsigned()) {
        return 0;
      }
      return it->get<std::uint32_t>();
    }

    [[nodiscard]] bool jsonBool(const nlohmann::json& json, std::string_view key) {
      const auto it = json.find(key);
      return it != json.end() && it->is_boolean() && it->get<bool>();
    }

    // Calls `visit` for each object in the array at `key`, skipping anything else.
    template <typename Visit> void forEachObject(const nlohmann::json& json, std::string_view key, Visit&& visit) {
      const auto it = json.find(key);
      if (it == json.end() || !it->is_array()) {
        return;
      }
      for (const auto& entry : *it) {
        if (entry.is_object()) {
          visit(entry);
        }
      }
    }

    // The strings in the array at `key`.
    [[nodiscard]] std::vector<std::string> jsonStrings(const nlohmann::json& json, std::string_view key) {
      std::vector<std::string> out;
      if (const auto it = json.find(key); it != json.end() && it->is_array()) {
        for (const auto& entry : *it) {
          if (entry.is_string()) {
            out.push_back(entry.get<std::string>());
          }
        }
      }
      return out;
    }

  } // namespace

  std::string workspaceIdFor(std::string_view output, std::uint32_t zeroBasedIndex) {
    return std::string(output) + ":" + std::to_string(zeroBasedIndex + 1);
  }

  Paw2Snapshot parseSnapshot(const nlohmann::json& message) {
    Paw2Snapshot snapshot;

    forEachObject(message, "outputs", [&snapshot](const nlohmann::json& entry) {
      Paw2Output output;
      output.name = jsonString(entry, "name");
      output.focused = jsonBool(entry, "focused");
      output.activeWorkspace = jsonIndex(entry, "active");
      output.layout = jsonString(entry, "layout");
      if (const auto occupied = entry.find("occupied"); occupied != entry.end() && occupied->is_array()) {
        for (const auto& flag : *occupied) {
          output.occupied.push_back(flag.is_boolean() && flag.get<bool>());
        }
      }
      forEachObject(entry, "modes", [&output](const nlohmann::json& mode) {
        output.modes.push_back(
            Paw2OutputMode{
                .width = mode.value("width", 0),
                .height = mode.value("height", 0),
                .refresh = mode.value("refresh", 0.0),
                .current = jsonBool(mode, "current"),
            }
        );
      });
      if (!output.name.empty()) {
        snapshot.outputs.push_back(std::move(output));
      }
    });

    forEachObject(message, "windows", [&snapshot](const nlohmann::json& entry) {
      const auto id = entry.find("id");
      if (id == entry.end() || !id->is_number_unsigned()) {
        return;
      }
      snapshot.windows.push_back(
          Paw2Window{
              .id = id->get<std::uint64_t>(),
              .identifier = jsonString(entry, "identifier"),
              .title = jsonString(entry, "title"),
              .appId = jsonString(entry, "app_id"),
              .output = jsonString(entry, "output"),
              .workspace = jsonIndex(entry, "workspace"),
              .focused = jsonBool(entry, "focused"),
              .minimized = jsonBool(entry, "minimized"),
          }
      );
    });

    forEachObject(message, "scratchpads", [&snapshot](const nlohmann::json& entry) {
      snapshot.scratchpads.push_back(
          Paw2Scratchpad{
              .name = jsonString(entry, "name"),
              .appId = jsonString(entry, "app_id"),
              .loaded = jsonBool(entry, "loaded"),
              .visible = jsonBool(entry, "visible"),
              .output = jsonString(entry, "output"),
          }
      );
    });

    snapshot.layouts = jsonStrings(message, "layouts");

    return snapshot;
  }

  Paw2Runtime::~Paw2Runtime() { closeSocket(false); }

  bool Paw2Runtime::available() const {
    ensureResolved();
    return !m_socketPath.empty();
  }

  const Paw2Window* Paw2Runtime::findWindowByIdentifier(std::string_view identifier) const {
    if (identifier.empty()) {
      return nullptr;
    }
    const auto it = std::ranges::find(m_snapshot.windows, identifier, &Paw2Window::identifier);
    return it != m_snapshot.windows.end() ? &*it : nullptr;
  }

  const Paw2Window* Paw2Runtime::focusedWindow() const {
    const auto it = std::ranges::find_if(m_snapshot.windows, &Paw2Window::focused);
    return it != m_snapshot.windows.end() ? &*it : nullptr;
  }

  const Paw2Output* Paw2Runtime::focusedOutput() const {
    const auto it = std::ranges::find_if(m_snapshot.outputs, &Paw2Output::focused);
    return it != m_snapshot.outputs.end() ? &*it : nullptr;
  }

  const Paw2Output* Paw2Runtime::findOutput(std::string_view name) const {
    const auto it = std::ranges::find(m_snapshot.outputs, name, &Paw2Output::name);
    return it != m_snapshot.outputs.end() ? &*it : nullptr;
  }

  Signal<>::ScopedConnection Paw2Runtime::addListener(Listener listener) {
    auto connection = m_changed.connect(std::move(listener));
    connectIfNeeded();
    return connection;
  }

  bool Paw2Runtime::focusWorkspace(std::string_view output, std::uint32_t zeroBasedIndex) {
    nlohmann::json command{{"cmd", "focus-workspace"}, {"index", zeroBasedIndex}};
    if (!output.empty()) {
      command["output"] = output;
    }
    return send(command);
  }

  bool Paw2Runtime::focusWindow(std::uint64_t id) { return send({{"cmd", "focus-window"}, {"id", id}}); }

  bool Paw2Runtime::closeWindow(std::uint64_t id) { return send({{"cmd", "close-window"}, {"id", id}}); }

  bool Paw2Runtime::minimizeWindow(std::uint64_t id) { return send({{"cmd", "minimize-window"}, {"id", id}}); }

  bool Paw2Runtime::setLayout(std::string_view layout, std::string_view output) {
    nlohmann::json command{{"cmd", "set-layout"}};
    if (!layout.empty()) {
      command["layout"] = layout;
    }
    if (!output.empty()) {
      command["output"] = output;
    }
    return send(command);
  }

  bool Paw2Runtime::toggleScratchpad(std::string_view name) {
    return send({{"cmd", "toggle-scratchpad"}, {"name", name}});
  }

  bool Paw2Runtime::resetScratchpad(std::string_view name) {
    return send({{"cmd", "reset-scratchpad"}, {"name", name}});
  }

  bool Paw2Runtime::toggleScratchpadOverview() { return send({{"cmd", "toggle-scratchpad-overview"}}); }

  bool Paw2Runtime::toggleOverview() { return send({{"cmd", "toggle-overview"}}); }

  bool Paw2Runtime::quit() { return send({{"cmd", "quit"}}); }

  bool Paw2Runtime::send(const nlohmann::json& command) {
    connectIfNeeded();
    if (m_fd < 0) {
      return false;
    }
    std::string payload = command.dump();
    payload.push_back('\n');
    std::size_t offset = 0;
    while (offset < payload.size()) {
      const ssize_t written = ::send(m_fd, payload.data() + offset, payload.size() - offset, MSG_NOSIGNAL);
      if (written > 0) {
        offset += static_cast<std::size_t>(written);
        continue;
      }
      if (written < 0 && errno == EINTR) {
        continue;
      }
      // Commands are a few dozen bytes; a full send buffer means paw2 has stopped reading.
      kLog.warn("paw2 control socket write failed: {}", std::strerror(errno));
      closeSocket(true);
      return false;
    }
    return true;
  }

  int Paw2Runtime::pollTimeoutMs() const noexcept {
    if (m_fd >= 0 || !available()) {
      return -1;
    }
    if (m_nextReconnectAt.time_since_epoch().count() == 0) {
      return 0;
    }
    const auto remaining =
        std::chrono::ceil<std::chrono::milliseconds>(m_nextReconnectAt - std::chrono::steady_clock::now()).count();
    return static_cast<int>(std::max<std::int64_t>(0, remaining));
  }

  void Paw2Runtime::dispatchPoll(short revents) {
    if (!available()) {
      return;
    }
    if (m_fd < 0) {
      connectIfNeeded();
      return;
    }
    if ((revents & POLLIN) != 0) {
      readSocket();
    }
    if (m_fd >= 0 && (revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      closeSocket(true);
    }
  }

  void Paw2Runtime::cleanup() {
    closeSocket(false);
    m_readBuffer.clear();
    m_reconnectBackoff = kReconnectInitial;
  }

  void Paw2Runtime::ensureResolved() const {
    if (m_resolved) {
      return;
    }
    m_resolved = true;
    if (const char* path = std::getenv("PAW2_SOCKET"); path != nullptr && path[0] != '\0') {
      m_socketPath = path;
    }
  }

  void Paw2Runtime::connectIfNeeded() {
    ensureResolved();
    if (m_fd >= 0 || m_socketPath.empty()) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (m_nextReconnectAt.time_since_epoch().count() != 0 && now < m_nextReconnectAt) {
      return;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (m_socketPath.size() >= sizeof(addr.sun_path)) {
      kLog.warn("PAW2_SOCKET path too long");
      m_socketPath.clear();
      return;
    }
    std::memcpy(addr.sun_path, m_socketPath.c_str(), m_socketPath.size() + 1);

    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
      scheduleReconnect();
      return;
    }
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
      kLog.debug("paw2 control socket connect failed: {}", std::strerror(errno));
      ::close(fd);
      scheduleReconnect();
      return;
    }
    if (const int flags = ::fcntl(fd, F_GETFL, 0); flags >= 0) {
      (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    m_fd = fd;
    m_nextReconnectAt = {};
    m_reconnectBackoff = kReconnectInitial;
    m_readBuffer.clear();
    kLog.info("connected to paw2 control socket");
  }

  void Paw2Runtime::closeSocket(bool scheduleReconnectFlag) {
    if (m_fd >= 0) {
      ::close(m_fd);
      m_fd = -1;
      // Stale state is worse than none: the bar would keep drawing windows that may be gone.
      const bool hadState = m_hasSnapshot;
      m_snapshot = {};
      m_hasSnapshot = false;
      m_trusted = false;
      // Only a real disconnect is news. Without a reconnect this is teardown, and the listeners'
      // owners (bar widgets, the platform) are already being destroyed.
      if (hadState && scheduleReconnectFlag) {
        notify();
      }
    }
    if (scheduleReconnectFlag) {
      scheduleReconnect();
    } else {
      m_nextReconnectAt = {};
    }
  }

  void Paw2Runtime::scheduleReconnect() {
    m_nextReconnectAt = std::chrono::steady_clock::now() + m_reconnectBackoff;
    m_reconnectBackoff = std::min(m_reconnectBackoff * 2, std::chrono::seconds(kReconnectMax));
  }

  void Paw2Runtime::readSocket() {
    std::array<char, 8192> buffer{};
    while (true) {
      const ssize_t readBytes = ::read(m_fd, buffer.data(), buffer.size());
      if (readBytes > 0) {
        m_readBuffer.insert(m_readBuffer.end(), buffer.begin(), buffer.begin() + readBytes);
        if (m_readBuffer.size() > kReadBufferMaxBytes) {
          kLog.warn("paw2 control socket buffer exceeded {} bytes; reconnecting", kReadBufferMaxBytes);
          m_readBuffer.clear();
          closeSocket(true);
          return;
        }
        continue;
      }
      if (readBytes == 0) {
        parseMessages();
        closeSocket(true);
        return;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      closeSocket(true);
      return;
    }
    parseMessages();
  }

  void Paw2Runtime::parseMessages() {
    auto lineStart = m_readBuffer.begin();
    for (auto it = m_readBuffer.begin(); it != m_readBuffer.end(); ++it) {
      if (*it != '\n') {
        continue;
      }
      std::string_view line(&*lineStart, static_cast<std::size_t>(std::distance(lineStart, it)));
      if (!line.empty()) {
        handleMessage(line);
      }
      lineStart = std::next(it);
    }
    m_readBuffer.erase(m_readBuffer.begin(), lineStart);
  }

  void Paw2Runtime::handleMessage(std::string_view line) {
    nlohmann::json json;
    try {
      json = nlohmann::json::parse(line);
    } catch (const nlohmann::json::exception& e) {
      kLog.warn("unparseable paw2 control message: {}", e.what());
      return;
    }
    if (!json.is_object()) {
      return;
    }

    const std::string type = jsonString(json, "type");
    if (type == "snapshot") {
      auto next = parseSnapshot(json);
      if (m_hasSnapshot && next == m_snapshot) {
        return;
      }
      m_snapshot = std::move(next);
      m_hasSnapshot = true;
      notify();
      return;
    }
    if (type == "config") {
      m_trusted = true;
      m_config = json.value("values", nlohmann::json::object());
      m_configOk = json.value("ok", true);
      m_configErrors = jsonStrings(json, "errors");
      ++m_configGeneration;
      notify();
    }
  }

  void Paw2Runtime::notify() const { m_changed.emit(); }

} // namespace compositors::paw2
