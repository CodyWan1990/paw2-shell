pragma Singleton

import QtQuick
import Quickshell.Io
import "." as Services

/// Package update counter — polls the system package manager (pacman or dnf) + flatpak every 12 hours.
/// pacman path needs checkupdates (pacman-contrib); dnf path uses `dnf check-update`.
QtObject {
    readonly property int count: internal.count
    readonly property var packages: internal.packages

    readonly property int flatpakCount: internal.flatpakCount
    readonly property var flatpakPackages: internal.flatpakPackages

    readonly property int totalCount: count + flatpakCount
    readonly property bool checkingSystem: internal.checkingPacman
    readonly property bool checkingFlatpak: internal.checkingFlatpak
    readonly property bool checking: internal.checkingPacman || internal.checkingFlatpak

    readonly property color statusColor: {
        if (totalCount === 0) return Services.Colors.green;
        if (totalCount <= 10) return Services.Colors.yellow;
        if (totalCount <= 50) return Services.Colors.orange;
        return Services.Colors.red;
    }

    function refresh() {
        if (internal.checkingPacman || internal.checkingFlatpak) return;
        internal.checkingPacman = true;
        internal.checkingFlatpak = true;
        internal.pacmanProc.running = true;
        internal.flatpakProc.running = true;
    }

    function upgradeSystem() {
        internal.pacmanUpgradeProc.running = true;
    }

    function upgradeFlatpaks() {
        internal.flatpakUpgradeProc.running = true;
    }

    readonly property QtObject internal: QtObject {
        id: _internal
        property int count: 0
        property var packages: []
        property bool checkingPacman: false

        property int flatpakCount: 0
        property var flatpakPackages: []
        property bool checkingFlatpak: false

        // System package manager — detects pacman (checkupdates) or dnf at runtime.
        property Process pacmanProc: Process {
            command: ["sh", "-c",
                "if command -v checkupdates >/dev/null 2>&1; then " +
                    "pkgs=$(timeout 120s checkupdates 2>/dev/null); " +
                    "if [ -z \"$pkgs\" ]; then echo '[]'; exit 0; fi; " +
                    "echo \"$pkgs\" | awk '{printf \"%s\\t%s\\t%s\\n\", $1, $2, $4}' | " +
                    "jq -Rsc '[split(\"\\n\")[] | select(length>0) | split(\"\\t\") | {name:.[0], current:.[1], new:.[2]}]'; " +
                "elif command -v dnf >/dev/null 2>&1; then " +
                    "pkgs=$(timeout 120s dnf --refresh --assumeno check-update 2>/dev/null); rc=$?; " +
                    // dnf exits 100 when updates are available, 0 when none
                    "if [ \"$rc\" != \"100\" ] || [ -z \"$pkgs\" ]; then echo '[]'; exit 0; fi; " +
                    "echo \"$pkgs\" | awk 'NF==3 && $1 ~ /\\./ {name=$1; sub(/\\.[^.]*$/,\"\",name); printf \"%s\\t%s\\n\", name, $2}' | " +
                    "jq -Rsc '[split(\"\\n\")[] | select(length>0) | split(\"\\t\") | {name:.[0], current:\"\", new:.[1]}]'; " +
                "else echo '[]'; fi"
            ]
            stdout: StdioCollector {
                onStreamFinished: {
                    _internal.checkingPacman = false;
                    try {
                        let pkgs = JSON.parse(this.text.trim());
                        if (!Array.isArray(pkgs)) pkgs = [];
                        _internal.packages = pkgs;
                        _internal.count = pkgs.length;
                    } catch (e) {
                        console.warn("Updates: pacman parse error:", e);
                        _internal.count = 0;
                        _internal.packages = [];
                    }
                }
            }
        }

        // Flatpak (optional — skipped if not installed)
        property Process flatpakProc: Process {
            command: ["sh", "-c",
                "command -v flatpak >/dev/null 2>&1 || { echo '[]'; exit 0; }; " +
                "pkgs=$(timeout 120s flatpak remote-ls --updates --columns=name,application 2>/dev/null); " +
                "if [ -z \"$pkgs\" ]; then echo '[]'; exit 0; fi; " +
                "echo \"$pkgs\" | awk -F'\\t' '{printf \"%s\\t%s\\n\", $1, $2}' | " +
                "jq -Rsc '[split(\"\\n\")[] | select(length>0) | split(\"\\t\") | {name:.[0], id:.[1]}]'"
            ]
            stdout: StdioCollector {
                onStreamFinished: {
                    _internal.checkingFlatpak = false;
                    try {
                        let pkgs = JSON.parse(this.text.trim());
                        if (!Array.isArray(pkgs)) pkgs = [];
                        _internal.flatpakPackages = pkgs;
                        _internal.flatpakCount = pkgs.length;
                    } catch (e) {
                        console.warn("Updates: flatpak parse error:", e);
                        _internal.flatpakCount = 0;
                        _internal.flatpakPackages = [];
                    }
                }
            }
        }

        // Upgrade processes (refresh on exit)
        // DefaultApps knows how the chosen terminal spells "run this command"
        // and "tag the window"; the old inline flags were kitty-only.
        property Process pacmanUpgradeProc: Process {
            command: Services.DefaultApps.terminalArgv(["bash", "-c",
                "if command -v checkupdates >/dev/null 2>&1; then sudo pacman -Syu; " +
                "elif command -v dnf >/dev/null 2>&1; then sudo dnf --refresh upgrade; " +
                "else echo 'No supported package manager found.'; fi; " +
                "echo; echo Done.; read -rp 'Press Enter to exit...'"], "updatescratch")
            stdout: StdioCollector {
                onStreamFinished: {
                    _internal.checkingPacman = true;
                    _internal.pacmanProc.running = true;
                }
            }
        }

        property Process flatpakUpgradeProc: Process {
            command: Services.DefaultApps.terminalArgv(["bash", "-c",
                "flatpak update; echo; echo Done.; read -rp 'Press Enter to exit...'"],
                "updatescratch")
            stdout: StdioCollector {
                onStreamFinished: {
                    _internal.checkingFlatpak = true;
                    _internal.flatpakProc.running = true;
                }
            }
        }

        property Timer pollTimer: Timer {
            interval: 43200000  // 12 hours
            running: true
            repeat: true
            onTriggered: {
                _internal.checkingPacman = true;
                _internal.checkingFlatpak = true;
                _internal.pacmanProc.running = true;
                _internal.flatpakProc.running = true;
            }
        }

        Component.onCompleted: {
            checkingPacman = true;
            checkingFlatpak = true;
            pacmanProc.running = true;
            flatpakProc.running = true;
        }
    }
}
