pragma Singleton

import QtQuick
import Quickshell
import "." as Services

/// Default applications, detected rather than typed.
///
/// Everything here comes from installed desktop entries: `DesktopEntries`
/// gives a parsed `command` argv with the field codes (%U, %f, …) already
/// stripped, so nothing in the shell has to parse an `Exec=` line or build a
/// shell string — the two ways this normally goes wrong. Spawns are argv
/// arrays; no value from here is ever interpolated into `sh -c`.
QtObject {
    id: root

    /// Entries that call themselves terminals / text editors, minus the ones
    /// marked NoDisplay (alternatives entries, kiosk wrappers).
    readonly property var terminals: internal.byCategory("TerminalEmulator")
    readonly property var editors: internal.byCategory("TextEditor")

    /// Display names, for a dropdown model.
    readonly property var terminalNames: terminals.map(e => e.name)
    readonly property var editorNames: editors.map(e => e.name)

    readonly property var terminalEntry: internal.find(terminals,
        Services.Settings.appTerminalId)
    readonly property var editorEntry: internal.find(editors,
        Services.Settings.appEditorId)

    /// Name to show as the current choice. Falls back to the legacy free-text
    /// `appTerminal` so an existing settings.json keeps working.
    readonly property string terminalName: terminalEntry ? terminalEntry.name
        : (Services.Settings.appTerminal || "")
    readonly property string editorName: editorEntry ? editorEntry.name : ""

    /// argv[0] of the chosen terminal — what a keybind's `spawn` needs.
    readonly property string terminalCommand: terminalEntry
        ? (terminalEntry.command[0] ?? "")
        : (Services.Settings.appTerminal || "")

    /// Pick by display name (what the dropdowns hand back).
    ///
    /// This sets the terminal the shell itself spawns. It deliberately does not
    /// touch paw2's terminal keybind: `[bindings]` belongs to
    /// `config.d/keybinds.toml`, and a drop-in owns every table it names, so
    /// writing a binding from here would make `shell.toml` take the whole
    /// section over from the keybinds editor.
    function setTerminalByName(name) {
        const e = internal.byName(terminals, name);
        if (!e)
            return;
        Services.Settings.appTerminalId = e.id;
        Services.Settings.appTerminal = e.command[0] ?? "";
    }

    function setEditorByName(name) {
        const e = internal.byName(editors, name);
        if (e)
            Services.Settings.appEditorId = e.id;
    }

    /// argv running `cmd` (an argv array) inside the chosen terminal.
    /// `appId`, when given, asks the terminal to tag the window so window
    /// rules can match it — only for terminals that support saying so.
    function terminalArgv(cmd, appId) {
        const entry = root.terminalEntry;
        const base = entry ? entry.command.slice() : [root.terminalCommand || "kitty"];
        const spec = internal.termSpec(base[0]);
        let argv = base;
        if (appId && spec.classFlag)
            argv = argv.concat([spec.classFlag + appId]);
        return argv.concat(spec.run, cmd);
    }

    /// argv opening `path` in the chosen editor, wrapped in the terminal when
    /// the entry says it is a console program. Empty when no editor is set.
    function editorArgv(path) {
        const e = root.editorEntry;
        if (!e)
            return [];
        const argv = e.command.concat([path]);
        return e.runInTerminal ? terminalArgv(argv, "") : argv;
    }

    readonly property QtObject internal: QtObject {
        id: internal

        // Terminals agree on `-e` far more often than not, so that is the
        // default and only the disagreements are listed. `classFlag` is a
        // prefix because these all take --flag=value.
        readonly property var termSpecs: ({
            "gnome-terminal": { run: ["--"], classFlag: "" },
            "ptyxis":         { run: ["--"], classFlag: "" },
            "tilix":          { run: ["-e"], classFlag: "" },
            "wezterm":        { run: ["start", "--"], classFlag: "--class=" },
            "foot":           { run: ["-e"], classFlag: "--app-id=" },
            "kitty":          { run: ["-e"], classFlag: "--class=" },
            "alacritty":      { run: ["-e"], classFlag: "--class=" }
        })
        readonly property var termDefault: ({ run: ["-e"], classFlag: "" })

        function termSpec(command) {
            // Match on the basename: an entry may be /usr/bin/kitty.
            const base = String(command ?? "").split("/").pop();
            return termSpecs[base] ?? termDefault;
        }

        function byCategory(category) {
            const all = DesktopEntries.applications.values;
            let out = [];
            for (const e of all) {
                if (e.noDisplay || !e.command || e.command.length === 0)
                    continue;
                if ((e.categories ?? []).indexOf(category) >= 0)
                    out.push(e);
            }
            out.sort((a, b) => a.name.localeCompare(b.name));
            return out;
        }

        function find(list, id) {
            if (!id)
                return null;
            for (const e of list)
                if (e.id === id)
                    return e;
            return null;
        }

        function byName(list, name) {
            for (const e of list)
                if (e.name === name)
                    return e;
            return null;
        }
    }
}
