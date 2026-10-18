#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <string>
#include <string_view>
#include <vector>

namespace compositors::paw2 {

  // One window as paw2's control socket reports it. `id` is paw2's own handle (what every window
  // command takes); `identifier` is the ext-foreign-toplevel-list identifier for the same window,
  // which is what lets the shell join the two views exactly. Both title and identifier come back
  // empty for a peer paw2 does not trust (see paw2's ipc.rs).
  struct Paw2Window {
    std::uint64_t id = 0;
    std::string identifier;
    std::string title;
    std::string appId;
    std::string output;
    // 0-based on the wire; ext-workspace ids are "<output>:<index + 1>".
    std::uint32_t workspace = 0;
    bool focused = false;
    // Out of its workspace until restored; focusing it restores it.
    bool minimized = false;

    bool operator==(const Paw2Window&) const = default;
  };

  // A mode a display advertises.
  struct Paw2OutputMode {
    std::int32_t width = 0;
    std::int32_t height = 0;
    double refresh = 0.0; // Hz
    bool current = false;

    bool operator==(const Paw2OutputMode&) const = default;
  };

  struct Paw2Output {
    std::string name;
    bool focused = false;
    std::uint32_t activeWorkspace = 0;
    std::vector<bool> occupied;
    std::string layout;
    std::vector<Paw2OutputMode> modes;

    bool operator==(const Paw2Output&) const = default;
  };

  struct Paw2Scratchpad {
    std::string name;
    std::string appId;
    bool loaded = false;
    bool visible = false;
    std::string output;

    bool operator==(const Paw2Scratchpad&) const = default;
  };

  struct Paw2Snapshot {
    std::vector<Paw2Output> outputs;
    std::vector<Paw2Window> windows;
    std::vector<Paw2Scratchpad> scratchpads;
    // Layout names in cycle order.
    std::vector<std::string> layouts;

    bool operator==(const Paw2Snapshot&) const = default;
  };

  // ext-workspace id paw2 assigns to a workspace: "<output>:<index + 1>".
  [[nodiscard]] std::string workspaceIdFor(std::string_view output, std::uint32_t zeroBasedIndex);

  // Parses one `snapshot` message. Exposed for tests.
  [[nodiscard]] Paw2Snapshot parseSnapshot(const nlohmann::json& message);

  // Owns the connection to $PAW2_SOCKET: a single persistent JSON-lines stream. paw2 pushes a
  // `snapshot` (and, to trusted peers, a `config`) on connect and after every change; commands go
  // out on the same socket and have no reply. Listeners are told after each message that changed
  // what they can read.
  class Paw2Runtime {
  public:
    using Listener = std::function<void()>;

    Paw2Runtime() = default;
    ~Paw2Runtime();

    Paw2Runtime(const Paw2Runtime&) = delete;
    Paw2Runtime& operator=(const Paw2Runtime&) = delete;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool connected() const noexcept { return m_fd >= 0; }
    [[nodiscard]] const Paw2Snapshot& snapshot() const noexcept { return m_snapshot; }
    [[nodiscard]] bool hasSnapshot() const noexcept { return m_hasSnapshot; }
    // paw2 sends `config` only to peers it trusts (installed by the package manager or listed
    // in [security] privileged_clients); an untrusted peer also gets blank titles and app ids.
    [[nodiscard]] bool trusted() const noexcept { return m_trusted; }
    // paw2's merged config (compiled defaults, config.toml, config.d/). Only sent to trusted peers.
    [[nodiscard]] const nlohmann::json& config() const noexcept { return m_config; }
    [[nodiscard]] bool configOk() const noexcept { return m_configOk; }
    [[nodiscard]] const std::vector<std::string>& configErrors() const noexcept { return m_configErrors; }
    // Bumped on every `config` message - paw2 sends one after each reload, so a writer can tell
    // the verdict on its own write from the config it already had.
    [[nodiscard]] std::uint64_t configGeneration() const noexcept { return m_configGeneration; }

    [[nodiscard]] const Paw2Window* findWindowByIdentifier(std::string_view identifier) const;
    [[nodiscard]] const Paw2Window* focusedWindow() const;
    [[nodiscard]] const Paw2Output* focusedOutput() const;
    [[nodiscard]] const Paw2Output* findOutput(std::string_view name) const;

    // Returns a token for removeListener().
    std::size_t addListener(Listener listener);
    void removeListener(std::size_t token);

    bool focusWorkspace(std::string_view output, std::uint32_t zeroBasedIndex);
    bool focusWindow(std::uint64_t id);
    bool closeWindow(std::uint64_t id);
    bool minimizeWindow(std::uint64_t id);
    // An empty name cycles to the next layout.
    bool setLayout(std::string_view layout, std::string_view output = {});
    bool toggleScratchpad(std::string_view name);
    bool toggleScratchpadOverview();
    bool toggleOverview();
    // Refused by paw2 for untrusted peers; callers fall back to logind.
    bool quit();

    [[nodiscard]] int pollFd() const noexcept { return m_fd; }
    [[nodiscard]] short pollEvents() const noexcept { return POLLIN | POLLHUP | POLLERR; }
    [[nodiscard]] int pollTimeoutMs() const noexcept;
    void dispatchPoll(short revents);
    void cleanup();

  private:
    bool send(const nlohmann::json& command);
    void ensureResolved() const;
    void connectIfNeeded();
    void closeSocket(bool scheduleReconnect);
    void scheduleReconnect();
    void readSocket();
    void parseMessages();
    void handleMessage(std::string_view line);
    void notify() const;

    mutable bool m_resolved = false;
    mutable std::string m_socketPath;
    int m_fd = -1;
    std::vector<char> m_readBuffer;
    std::chrono::steady_clock::time_point m_nextReconnectAt;
    std::chrono::seconds m_reconnectBackoff{1};

    Paw2Snapshot m_snapshot;
    bool m_hasSnapshot = false;
    bool m_trusted = false;
    nlohmann::json m_config = nlohmann::json::object();
    bool m_configOk = true;
    std::vector<std::string> m_configErrors;
    std::uint64_t m_configGeneration = 0;

    std::vector<std::pair<std::size_t, Listener>> m_listeners;
    std::size_t m_nextListenerToken = 1;
  };

} // namespace compositors::paw2
