import QtQuick
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// System uptime widget — reads /proc/uptime and formats as Xd Xh Xm.
Components.BarWidget {
    id: root

    icon: "uptime"
    defaultAccent: Services.Colors.uptimeUnderline
    text: root.uptimeStr

    property string uptimeStr: "..."

    // Fixed modes rather than a template: the default "auto" drops leading zero
    // units, which a placeholder string can't express.
    function formatUptime(seconds) {
        let s = Math.floor(seconds);
        let d = Math.floor(s / 86400);
        let h = Math.floor((s % 86400) / 3600);
        let m = Math.floor((s % 3600) / 60);
        switch (root.instance.format ?? "auto") {
        case "dhm": return d + "d " + h + "h " + m + "m";
        case "hm":  return Math.floor(s / 3600) + "h " + m + "m";
        case "d":   return d + "d";
        default:
            if (d > 0) return d + "d " + h + "h " + m + "m";
            if (h > 0) return h + "h " + m + "m";
            return m + "m";
        }
    }

    // Native file read (no Process spawn). /proc has no inotify, so re-read on a timer.
    FileView {
        id: uptimeView
        path: "file:///proc/uptime"
        onLoaded: {
            try {
                root.uptimeStr = root.formatUptime(parseFloat(this.text().trim().split(" ")[0]));
            } catch (e) {
                console.warn("Uptime: parse error:", e);
            }
        }
    }

    Timer {
        interval: (root.instance.intervalSec ?? 60) * 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: uptimeView.reload()
    }
}
