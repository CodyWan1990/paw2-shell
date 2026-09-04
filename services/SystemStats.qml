pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import "." as Services
import "../components" as Components

/// CPU, memory, and network stats. CPU still goes through cpu-stats.sh (the
/// hwmon sensor walk needs a glob); memory and net are read straight from /proc.
QtObject {
    readonly property real cpuPercent: internal.cpuPercent
    readonly property string cpuFreqGhz: internal.cpuFreqGhz
    readonly property int cpuTemp: internal.cpuTemp
    readonly property string cpuModel: internal.cpuModel
    readonly property int cpuCores: internal.cpuCores
    readonly property color cpuColor: Services.Colors.threshold(cpuPercent, 50, 65, 80)
    readonly property color cpuTempColor: Services.Colors.threshold(cpuTemp, 65, 75, 100)

    readonly property int memUsedGb: internal.memUsedGb
    readonly property int memTotalGb: internal.memTotalGb
    readonly property int memFreeGb: internal.memFreeGb
    readonly property int memPercent: internal.memPercent
    readonly property color memColor: Services.Colors.threshold(memPercent, 50, 65, 80)

    readonly property string netRx: internal.netRx
    readonly property string netTx: internal.netTx

    readonly property QtObject internal: QtObject {
        property real cpuPercent: 0
        property string cpuFreqGhz: "0.00"
        property int cpuTemp: 0
        property string cpuModel: ""
        property int cpuCores: 0
        property int memUsedGb: 0
        property int memTotalGb: 0
        property int memFreeGb: 0
        property int memPercent: 0

        property string netRx: "0B/s"
        property string netTx: "0B/s"

        property string scriptsDir: Services.Paths.scriptsDir

        // Previous raw net counters; /proc gives totals since boot, so the
        // rate is the delta between consecutive polls.
        property real prevRx: -1
        property real prevTx: -1
        property real prevMs: 0

        function humanRate(bps) {
            if (bps < 1024) return Math.round(bps) + "B/s";
            if (bps < 1048576) return (bps / 1024).toFixed(1) + "KB/s";
            if (bps < 1073741824) return (bps / 1048576).toFixed(1) + "MB/s";
            return (bps / 1073741824).toFixed(1) + "GB/s";
        }

        // PolledProcess is an Item but needs no visual parent — its Timer/Process
        // run fine owned by a singleton; it also adds overlap-guard + kill-timeout.
        property Item cpuPoll: Components.PolledProcess {
            command: ["bash", internal.scriptsDir + "/cpu-stats.sh"]
            interval: 5000
            onOutput: text => {
                try {
                    let d = JSON.parse(text);
                    internal.cpuPercent = d.usage || 0;
                    internal.cpuFreqGhz = d.freqGhz || "0.00";
                    internal.cpuTemp = d.temp || 0;
                    internal.cpuModel = d.model || "";
                    internal.cpuCores = d.cores || 0;
                } catch (e) {
                    console.warn("SystemStats: cpu parse error:", e, "raw:", text);
                }
            }
        }

        // Memory and network come straight out of /proc — FileView reads those
        // fine despite them reporting st_size 0, so there is no reason to fork
        // a shell every few seconds just to run awk over them.
        property FileView meminfo: FileView { path: "/proc/meminfo"; blockLoading: true }
        property FileView netroute: FileView { path: "/proc/net/route"; blockLoading: true }
        property FileView netdev: FileView { path: "/proc/net/dev"; blockLoading: true }

        function readMem() {
            let total = 0, avail = 0;
            for (const line of meminfo.text().split("\n")) {
                let m = line.match(/^(MemTotal|MemAvailable):\s+(\d+)/);
                if (!m) continue;
                if (m[1] === "MemTotal") total = parseInt(m[2]);
                else avail = parseInt(m[2]);
            }
            if (total <= 0) return;
            const gb = kb => Math.round(kb / 1048576);
            memUsedGb = gb(total - avail);
            memFreeGb = gb(avail);
            memTotalGb = gb(total);
            memPercent = Math.round(100 * (total - avail) / total);
        }

        // Interface carrying the default route, i.e. what `ip route show
        // default` reports: the /proc/net/route row whose destination is 0.
        function defaultIface() {
            let lines = netroute.text().split("\n");
            for (let i = 1; i < lines.length; i++) {
                let f = lines[i].trim().split(/\s+/);
                if (f.length > 1 && f[1] === "00000000") return f[0];
            }
            return "";
        }

        function readNet() {
            let iface = defaultIface();
            if (!iface) return;
            // /proc/net/dev columns: iface: rx_bytes ... (8 fields) tx_bytes ...
            let row = netdev.text().split("\n").find(l => l.trim().startsWith(iface + ":"));
            if (!row) return;
            let f = row.split(":")[1].trim().split(/\s+/);
            let rx = parseInt(f[0]), tx = parseInt(f[8]), now = Date.now();
            // First sample or counter reset (iface change/reboot): re-baseline.
            if (prevRx >= 0 && rx >= prevRx && now > prevMs) {
                let dt = (now - prevMs) / 1000;
                netRx = humanRate((rx - prevRx) / dt);
                netTx = humanRate((tx - prevTx) / dt);
            }
            prevRx = rx;
            prevTx = tx;
            prevMs = now;
        }

        property Timer memTimer: Timer {
            interval: 5000; running: true; repeat: true; triggeredOnStart: true
            onTriggered: { internal.meminfo.reload(); internal.readMem(); }
        }

        property Timer netTimer: Timer {
            interval: 2000; running: true; repeat: true; triggeredOnStart: true
            onTriggered: {
                internal.netroute.reload();
                internal.netdev.reload();
                internal.readNet();
            }
        }

    }
}
