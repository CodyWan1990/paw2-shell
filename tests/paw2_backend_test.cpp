#include "compositors/paw2/paw2_runtime.h"
#include "compositors/paw2/paw2_workspace_backend.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

  bool check(bool cond, const char* msg) {
    if (!cond) {
      std::cerr << "FAIL: " << msg << '\n';
    }
    return cond;
  }

  // What paw2's ipc.rs serialises, field for field.
  constexpr const char* kConfig = R"({"type":"config","values":{"layout":{"gaps":8}},"ok":true,"errors":[]})";
  constexpr const char* kSnapshot =
      R"({"type":"snapshot","outputs":[)"
      R"({"name":"DP-1","focused":true,"active":1,"occupied":[true,true,false],"layout":"dwindle",)"
      R"("modes":[{"width":2560,"height":1440,"refresh":165.08,"current":true},{"width":1920,"height":1080,"refresh":60.0,"current":false}]},)"
      R"({"name":"HDMI-A-1","focused":false,"active":0,"occupied":[true,false,false],"layout":"scrolling","modes":[]}],)"
      R"("windows":[)"
      R"({"id":3,"identifier":"aaa","title":"term","app_id":"kitty","output":"DP-1","workspace":0,"focused":false},)"
      R"({"id":4,"identifier":"bbb","title":"web","app_id":"firefox","output":"DP-1","workspace":1,"focused":true},)"
      R"({"id":9,"identifier":"ccc","title":"chat","app_id":"vesktop","output":"HDMI-A-1","workspace":0,"focused":false}],)"
      R"("scratchpads":[{"name":"term","app_id":"kitty","loaded":true,"visible":false,"output":null}],)"
      R"("layouts":["scrolling","dwindle","master","monocle"]})";

  void pump(compositors::paw2::Paw2Runtime& runtime) {
    for (int i = 0; i < 50 && runtime.pollFd() >= 0; ++i) {
      pollfd fd{.fd = runtime.pollFd(), .events = runtime.pollEvents(), .revents = 0};
      if (::poll(&fd, 1, 20) > 0) {
        runtime.dispatchPoll(fd.revents);
        if (runtime.hasSnapshot()) {
          return;
        }
      }
    }
  }

} // namespace

int main() {
  bool ok = true;

  // Parsing on its own.
  const auto parsed = compositors::paw2::parseSnapshot(nlohmann::json::parse(kSnapshot));
  ok &= check(parsed.outputs.size() == 2, "two outputs");
  ok &= check(parsed.windows.size() == 3, "three windows");
  ok &= check(parsed.layouts.size() == 4 && parsed.layouts[1] == "dwindle", "layout list");
  ok &= check(parsed.scratchpads.size() == 1 && parsed.scratchpads[0].output.empty(), "null scratchpad output");
  ok &= check(parsed.outputs[0].activeWorkspace == 1 && parsed.outputs[0].layout == "dwindle", "output fields");
  ok &= check(
      parsed.outputs[0].modes.size() == 2
          && parsed.outputs[0].modes[0].current
          && parsed.outputs[0].modes[0].refresh == 165.08
          && parsed.outputs[0].modes[1].width == 1920,
      "output modes"
  );
  ok &= check(compositors::paw2::workspaceIdFor("DP-1", 0) == "DP-1:1", "ext-workspace id is 1-based");

  // Live socket: paw2 pushes config + snapshot on connect, then reads commands.
  const std::string socketPath = "/tmp/paw2-shell-backend-test-" + std::to_string(getpid()) + ".sock";
  ::unlink(socketPath.c_str());
  const int listener = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, socketPath.c_str(), socketPath.size() + 1);
  if (!check(::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0, "bind")
      || !check(::listen(listener, 1) == 0, "listen")) {
    return 1;
  }

  std::string received;
  std::thread server([&]() {
    const int client = ::accept(listener, nullptr, nullptr);
    if (client < 0) {
      return;
    }
    const std::string greeting = std::string(kConfig) + "\n" + kSnapshot + "\n";
    (void)::write(client, greeting.data(), greeting.size());
    char buffer[1024];
    while (std::count(received.begin(), received.end(), '\n') < 2) {
      const ssize_t n = ::read(client, buffer, sizeof(buffer));
      if (n <= 0) {
        break;
      }
      received.append(buffer, static_cast<std::size_t>(n));
    }
    ::close(client);
  });

  ::setenv("PAW2_SOCKET", socketPath.c_str(), 1);
  compositors::paw2::Paw2Runtime runtime;
  Paw2WorkspaceBackend backend(runtime);
  int changes = 0;
  backend.setChangeCallback([&]() { ++changes; });
  ok &= check(runtime.connected(), "connects on construction");
  pump(runtime);

  ok &= check(runtime.hasSnapshot(), "snapshot received");
  ok &= check(runtime.trusted(), "config message marks the peer trusted");
  ok &= check(changes >= 1, "listener notified");
  ok &= check(runtime.focusedOutput() != nullptr && runtime.focusedOutput()->name == "DP-1", "focused output");
  ok &= check(backend.hasExactWindowIdentity(), "identifiers present means exact identity");
  ok &= check(backend.focusedWindowId() == std::optional<std::string>("bbb"), "focused window id");

  std::vector<Workspace> workspaces{
      {.id = "DP-1:1", .name = "1"}, {.id = "DP-1:2", .name = "2"}, {.id = "DP-1:3", .name = "3"}
  };
  backend.apply(workspaces, "DP-1");
  ok &= check(workspaces[0].occupied && workspaces[1].occupied && !workspaces[2].occupied, "occupancy");

  const auto dpWindows = backend.workspaceWindows("DP-1");
  ok &= check(dpWindows.size() == 2, "windows filtered by output");
  ok &= check(dpWindows[1].windowId == "bbb" && dpWindows[1].workspaceKey == "2", "window keyed by workspace name");
  const auto keys = backend.workspaceKeys("HDMI-A-1");
  ok &= check(keys == std::vector<std::string>{"1", "2", "3"}, "per-output workspace keys");
  ok &= check(backend.appIdsByWorkspace("HDMI-A-1").at("1") == std::vector<std::string>{"vesktop"}, "app ids");

  ok &= check(backend.focusWindowById("ccc"), "focus by identifier");
  ok &= check(!backend.closeWindowById("nope"), "unknown identifier is refused");
  ok &= check(backend.closeWindowById("aaa"), "close by identifier");
  server.join();

  const auto newline = received.find('\n');
  ok &= check(newline != std::string::npos, "commands written as lines");
  if (newline != std::string::npos) {
    const auto first = nlohmann::json::parse(received.substr(0, newline));
    const auto second = nlohmann::json::parse(received.substr(newline + 1, received.find('\n', newline + 1) - newline - 1));
    ok &= check(first == nlohmann::json{{"cmd", "focus-window"}, {"id", 9}}, "focus-window uses paw2's numeric id");
    ok &= check(second == nlohmann::json{{"cmd", "close-window"}, {"id", 3}}, "close-window uses paw2's numeric id");
  }

  // paw2 hangs up: state is dropped rather than left stale.
  pump(runtime);
  for (int i = 0; i < 50 && runtime.connected(); ++i) {
    pollfd fd{.fd = runtime.pollFd(), .events = runtime.pollEvents(), .revents = 0};
    if (::poll(&fd, 1, 20) > 0) {
      runtime.dispatchPoll(fd.revents);
    }
  }
  ok &= check(!runtime.connected() && !runtime.hasSnapshot() && !runtime.trusted(), "disconnect clears state");

  ::close(listener);
  ::unlink(socketPath.c_str());
  return ok ? 0 : 1;
}
