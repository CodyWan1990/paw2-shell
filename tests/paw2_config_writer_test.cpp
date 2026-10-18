#include "compositors/paw2/paw2_config_writer.h"
#include "compositors/paw2/paw2_runtime.h"
#include "core/toml.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

  bool check(bool cond, const char* msg) {
    if (!cond) {
      std::cerr << "FAIL: " << msg << '\n';
    }
    return cond;
  }

  // A stand-in for paw2: a listening socket, and the accepted peer to send `config` messages on.
  struct FakePaw2 {
    int listener = -1;
    int peer = -1;
    std::string path;

    FakePaw2() {
      path = (std::filesystem::temp_directory_path() / ("paw2-writer-test-" + std::to_string(getpid()) + ".sock"))
                 .string();
      ::unlink(path.c_str());
      listener = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
      sockaddr_un address{};
      address.sun_family = AF_UNIX;
      std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
      (void)::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
      (void)::listen(listener, 1);
      ::setenv("PAW2_SOCKET", path.c_str(), 1);
    }
    ~FakePaw2() {
      ::close(peer);
      ::close(listener);
      ::unlink(path.c_str());
    }
    void accept() { peer = ::accept(listener, nullptr, nullptr); }
    void sendConfig(const nlohmann::json& values, bool ok = true, std::vector<std::string> errors = {}) const {
      const std::string line =
          nlohmann::json{{"type", "config"}, {"values", values}, {"ok", ok}, {"errors", errors}}.dump() + "\n";
      (void)::write(peer, line.data(), line.size());
    }
  };

  void pump(compositors::paw2::Paw2Runtime& runtime) {
    for (int i = 0; i < 20 && runtime.pollFd() >= 0; ++i) {
      pollfd fd{.fd = runtime.pollFd(), .events = runtime.pollEvents(), .revents = 0};
      if (::poll(&fd, 1, 20) > 0) {
        runtime.dispatchPoll(fd.revents);
        return;
      }
    }
  }

  nlohmann::json readToml(const std::filesystem::path& path) {
    const toml::table table = toml::parse_file(path.string());
    std::stringstream json;
    json << toml::json_formatter{table};
    return nlohmann::json::parse(json.str());
  }

} // namespace

// paw2 wants exactly nine binding lists per workspace action.
nlohmann::json nine(const std::string& prefix) {
  nlohmann::json lists = nlohmann::json::array();
  for (int i = 1; i <= 9; ++i) {
    lists.push_back(nlohmann::json::array({prefix + std::to_string(i)}));
  }
  return lists;
}

int main() {
  using compositors::paw2::Paw2ConfigWriter;
  using compositors::paw2::Paw2Runtime;
  bool ok = true;

  const auto configDir = std::filesystem::temp_directory_path() / ("paw2-writer-test-" + std::to_string(getpid()));
  std::filesystem::remove_all(configDir);

  // paw2's merged config: what it would send after loading config.toml.
  nlohmann::json config = {
      {"appearance", {{"gaps", 15}, {"blur", true}, {"opacity", 0.95}, {"focus_ring_color", "#0b3d91"}}},
      {"animations", {{"enabled", true}, {"duration_ms", 150}}},
      {"monitors", nlohmann::json::array({{{"connector", "DP-1"}, {"scale", 1.0}, {"stray", 7}}})},
      {"bindings",
       {{"close_window", {"Super+q"}},
        {"commands", {{"Super+Return", {"kitty"}}}},
        {"workspace", {{"switch", nine("Super+")}, {"move", nine("Super+Shift+")}}}}},
  };

  FakePaw2 paw2;
  Paw2Runtime runtime;
  Paw2ConfigWriter writer(runtime, configDir);
  paw2.accept();

  // An edit before paw2's config arrives has nothing to seed its table from; it waits.
  writer.set("input.focus_follows_mouse", true);
  writer.flush();
  ok &= check(!std::filesystem::exists(configDir / "config.d" / "shell.toml"), "no write before paw2's config");

  paw2.sendConfig(config);
  pump(runtime);
  ok &= check(runtime.trusted() && runtime.configGeneration() == 1, "config received");

  // A slider hands back a double for an integer key; paw2 would reject `gaps = 20.0`.
  writer.set("appearance.gaps", 20.0);
  ok &= check(writer.get("appearance.gaps") == nlohmann::json(20), "pending edit is visible, as an integer");
  writer.flush();
  auto shell = readToml(configDir / "config.d" / "shell.toml");
  ok &= check(shell["appearance"]["gaps"] == 20, "edit written");
  ok &= check(shell["appearance"]["blur"] == true && shell["appearance"]["opacity"] == 0.95, "table claimed whole");
  ok &= check(!shell.contains("animations"), "untouched tables are not claimed");
  ok &= check(!shell.contains("bindings"), "shell.toml never names [bindings]");
  ok &= check(shell["input"]["focus_follows_mouse"] == true, "the early edit is written once config is in");

  // An edit made while paw2 is still answering the last write (a slider still moving).
  writer.set("appearance.corner_radius", 8);

  // paw2 accepts: its config now carries the value.
  config["appearance"]["gaps"] = 20;
  config["input"]["focus_follows_mouse"] = true;
  paw2.sendConfig(config);
  pump(runtime);
  ok &= check(writer.rejectedErrors().empty() && writer.owns("appearance.gaps"), "accepted write is kept");
  ok &= check(writer.get("appearance.corner_radius") == nlohmann::json(8), "the newer edit survives the verdict");
  writer.flush();
  ok &= check(readToml(configDir / "config.d" / "shell.toml")["appearance"]["corner_radius"] == 8, "and is written");
  config["appearance"]["corner_radius"] = 8;
  paw2.sendConfig(config);
  pump(runtime);

  // paw2 rejects the next write: the file goes back to what it last accepted.
  writer.set("appearance.gaps", 999);
  writer.flush();
  ok &= check(readToml(configDir / "config.d" / "shell.toml")["appearance"]["gaps"] == 999, "bad value written");
  paw2.sendConfig(config, false, {"appearance.gaps must be at most 256"});
  pump(runtime);
  ok &= check(readToml(configDir / "config.d" / "shell.toml")["appearance"]["gaps"] == 20, "rejected write rolled back");
  ok &= check(writer.rejectedErrors().size() == 1, "paw2's error kept for Settings");
  ok &= check(writer.get("appearance.gaps") == nlohmann::json(20), "pending edit dropped after rejection");

  // A config message we did not cause (a hand edit) is not ours to roll back.
  paw2.sendConfig(config, false, {"config.toml: broken"});
  pump(runtime);
  ok &= check(readToml(configDir / "config.d" / "shell.toml")["appearance"]["gaps"] == 20, "no rollback without a write");

  // Monitors: the drop-in carries every monitor, only paw2's keys, edits applied.
  writer.setMonitor("DP-2", "scale", 1.5);
  writer.flush();
  shell = readToml(configDir / "config.d" / "shell.toml");
  ok &= check(shell["monitors"].size() == 2, "all monitors carried");
  ok &= check(shell["monitors"][0]["connector"] == "DP-1" && !shell["monitors"][0].contains("stray"), "known keys only");
  ok &= check(shell["monitors"][1]["connector"] == "DP-2" && shell["monitors"][1]["scale"] == 1.5, "new monitor");
  paw2.sendConfig(config);
  pump(runtime);

  // Bindings: keybinds.toml carries the whole table, nested tables included.
  writer.setBinding("close_window", nlohmann::json::array({"Super+q", "Super+w"}));
  writer.setBinding("commands.Super+space", nlohmann::json::array({"paw2-shell", "msg", "panel-toggle", "launcher"}));
  writer.flush();
  const auto keybinds = readToml(configDir / "config.d" / "keybinds.toml");
  ok &= check(keybinds["bindings"]["close_window"].size() == 2, "binding edited");
  ok &= check(keybinds["bindings"]["commands"].size() == 2, "command added beside the existing one");
  ok &= check(keybinds["bindings"]["workspace"]["switch"].size() == 9, "untouched nested tables carried");
  ok &= check(!readToml(configDir / "config.d" / "shell.toml").contains("bindings"), "bindings stay out of shell.toml");

  // Reset to default drops the key.
  paw2.sendConfig(config);
  pump(runtime);
  writer.unset("appearance.blur");
  writer.flush();
  ok &= check(!readToml(configDir / "config.d" / "shell.toml")["appearance"].contains("blur"), "unset removes key");

  // A fresh writer picks up what earlier sessions wrote.
  {
    Paw2Runtime otherRuntime;
    Paw2ConfigWriter reloaded(otherRuntime, configDir);
    ok &= check(reloaded.owns("appearance.gaps") && !reloaded.owns("appearance.blur"), "drop-in reloaded");
  }

  // PAW2_WRITER_TEST_KEEP=1 leaves the drop-ins behind, to load them into a real paw2.
  if (std::getenv("PAW2_WRITER_TEST_KEEP") == nullptr) {
    std::filesystem::remove_all(configDir);
  } else {
    std::cerr << "kept " << configDir.string() << '\n';
  }
  return ok ? 0 : 1;
}
