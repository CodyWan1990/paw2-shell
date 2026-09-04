import QtQuick
import "../services" as Services
import "../components" as Components

/// Disk usage widget — shows used/total GB and percentage for a filesystem.
/// Tracks the root filesystem by default; set instance.path to track another mount.
Components.BarWidget {
    id: root

    icon: "disk"
    defaultAccent: Services.Colors.diskUnderline
    text: Services.Fmt.fill(instance.format ?? "{used}/{total}GB ({pct})", {
        used: root.used,
        total: root.total,
        free: root.freeGb,
        pct: "<font color='" + root.percentColor() + "'>" + root.percent + "%</font>"
    })

    readonly property string diskPath: (instance && instance.path) ? instance.path : "/"

    property string used: "?"
    property string total: "?"
    property int percent: 0
    // disk-stats.sh reports used/total only; the template's {free} is derived.
    readonly property string freeGb: {
        let u = parseFloat(used), t = parseFloat(total);
        return (isNaN(u) || isNaN(t)) ? "?" : String(Math.max(0, Math.round(t - u)));
    }

    function percentColor() {
        return Services.Colors.threshold(percent,
            instance.warnPct ?? 60, instance.highPct ?? 80, instance.critPct ?? 90);
    }

    Components.PolledProcess {
        command: [Services.Paths.script("disk-stats.sh"), root.diskPath]
        interval: (instance.intervalSec ?? 60) * 1000
        onOutput: text => {
            try {
                let j = JSON.parse(text.trim());
                root.used = j.used;
                root.total = j.total;
                root.percent = j.percent;
            } catch (e) {
                console.warn("DiskUsage: parse error:", e);
            }
        }
    }
}
