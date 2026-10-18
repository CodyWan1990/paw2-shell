#pragma once

#include "core/timer_manager.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compositors::paw2 {

  class Paw2Runtime;

  // Changes paw2's settings the way paw2 wants them changed: never in config.toml - that file and
  // its comments are the user's - but in two drop-ins paw2 merges over it and reloads by itself:
  //
  //   config.d/shell.toml     every Settings page except the shortcuts
  //   config.d/keybinds.toml  [bindings], and nothing else
  //
  // A drop-in owns every top-level table it names *whole*, which shapes everything here:
  //   - The first write into a table "claims" it: paw2's current values for the rest of that table
  //     are written too, or they would fall back to compiled defaults.
  //   - [[monitors]] is an array that merges by replacement, so once the drop-in carries one monitor
  //     it carries them all, re-seeded from paw2's effective list on every write.
  //   - [bindings] lives only in keybinds.toml, written whole from paw2's effective bindings plus
  //     the edit; shell.toml never names it, or it would take the table from the shortcuts page.
  //
  // paw2 validates; this class does not. After a write it waits for paw2's next `config` message:
  // a rejection rolls the file back to what paw2 last accepted and keeps paw2's errors for the UI.
  // Only our own writes are rolled back - a broken hand edit elsewhere is not ours to undo.
  //
  // Values are JSON (as paw2 sends its config); paths are dotted, e.g. "appearance.gaps".
  class Paw2ConfigWriter {
  public:
    using Listener = std::function<void()>;

    // `configDir` is paw2's config directory (holding config.toml); empty means
    // $XDG_CONFIG_HOME/paw2 or ~/.config/paw2.
    explicit Paw2ConfigWriter(Paw2Runtime& runtime, std::filesystem::path configDir = {});
    ~Paw2ConfigWriter();

    Paw2ConfigWriter(const Paw2ConfigWriter&) = delete;
    Paw2ConfigWriter& operator=(const Paw2ConfigWriter&) = delete;

    // paw2's effective value at `path`, with any edit still waiting for paw2's verdict on top.
    [[nodiscard]] std::optional<nlohmann::json> get(std::string_view path) const;
    // Whether shell.toml sets `path` (so Settings can offer a reset).
    [[nodiscard]] bool owns(std::string_view path) const;
    // Errors from paw2 rejecting our last write; empty once a write is accepted.
    [[nodiscard]] const std::vector<std::string>& rejectedErrors() const noexcept { return m_rejectedErrors; }

    void set(std::string path, nlohmann::json value);
    // Drop `path` from shell.toml; paw2 then uses its compiled default for it.
    void unset(std::string_view path);
    // One key of one monitor's [[monitors]] entry, created if paw2 has none for it yet.
    void setMonitor(std::string_view connector, std::string key, nlohmann::json value);
    // A key under [bindings] in keybinds.toml, e.g. "close_window" or "commands.Super+Return".
    // A null value removes it.
    void setBinding(std::string path, nlohmann::json value);

    // Write now instead of after the debounce (e.g. when Settings closes).
    void flush();

    // Whether keybinds.toml exists, and so owns [bindings] instead of config.toml.
    [[nodiscard]] bool ownsBindings() const;
    // Delete keybinds.toml, handing [bindings] back to config.toml.
    void releaseBindings();

    std::size_t addListener(Listener listener);
    void removeListener(std::size_t token);

    // ── Exposed for tests ───────────────────────────────────────────────────
    [[nodiscard]] static std::string renderTomlDocument(const nlohmann::json& document);
    [[nodiscard]] std::filesystem::path shellDropIn() const { return m_dropInDir / "shell.toml"; }
    [[nodiscard]] std::filesystem::path keybindsDropIn() const { return m_dropInDir / "keybinds.toml"; }

  private:
    void load();
    void scheduleSave();
    void save();
    void onRuntimeChanged();
    void notify() const;
    [[nodiscard]] const nlohmann::json& effectiveConfig() const;
    [[nodiscard]] bool claimed(std::string_view table) const;
    void claim(std::string_view table);
    [[nodiscard]] nlohmann::json shellDocument() const;
    [[nodiscard]] nlohmann::json monitorsWithEdits() const;
    [[nodiscard]] nlohmann::json bindingsWithEdits() const;
    bool writeFile(const std::filesystem::path& path, const nlohmann::json& document) const;

    Paw2Runtime& m_runtime;
    std::filesystem::path m_dropInDir;
    std::size_t m_runtimeListener = 0;

    // shell.toml as dotted paths: everything the shell has set or claimed.
    std::map<std::string, nlohmann::json> m_owned;
    std::map<std::string, nlohmann::json> m_lastGood;
    // Edits not yet written, or written but not yet confirmed by paw2.
    std::map<std::string, nlohmann::json> m_pending;
    std::vector<std::string> m_unsetPending;
    bool m_ownsMonitors = false;
    bool m_lastGoodOwnsMonitors = false;
    // connector -> key -> value
    std::map<std::string, std::map<std::string, nlohmann::json>> m_pendingMonitors;
    std::map<std::string, nlohmann::json> m_pendingBindings;

    bool m_shellDirty = false;
    bool m_keybindsDirty = false;
    bool m_awaitingVerdict = false;
    // A save came before paw2's config did, and runs once it arrives.
    bool m_waitingForConfig = false;
    std::uint64_t m_writtenAtGeneration = 0;
    std::optional<std::string> m_keybindsLastGood;
    std::vector<std::string> m_rejectedErrors;

    Timer m_saveTimer;
    std::vector<std::pair<std::size_t, Listener>> m_listeners;
    std::size_t m_nextListenerToken = 1;
  };

  // Splits a dotted path, e.g. "appearance.gaps" -> {"appearance", "gaps"}.
  [[nodiscard]] std::vector<std::string> splitPath(std::string_view path);

} // namespace compositors::paw2
