import QtQuick
import "../services" as Services
import "../components" as Components

/// Memory bar widget — shows used/total GB + usage %. Click opens detail popup.
Components.BarWidget {
    id: root

    icon: "memory"
    defaultAccent: Services.Colors.memoryUnderline
    clickable: true
    onClicked: memPopup.isOpen ? memPopup.close() : memPopup.open()

    // Per-instance thresholds, so two memory widgets can alarm at different points.
    readonly property color usageColor: Services.Colors.threshold(
        Services.SystemStats.memPercent,
        instance.warnPct ?? 50, instance.highPct ?? 65, instance.critPct ?? 80)

    text: {
        let pct = "<font color='" + root.usageColor + "'>"
            + Services.SystemStats.memPercent + "%</font>";
        return Services.Fmt.fill(instance.format ?? "{used}/{total}GB ({pct})", {
            used: Services.SystemStats.memUsedGb,
            total: Services.SystemStats.memTotalGb,
            free: Services.SystemStats.memFreeGb,
            pct: pct
        });
    }

    Components.WidgetPopup {
        id: memPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 300
        accentColor: Services.Colors.memoryUnderline

        Components.StatPopup {
            headerText: "🧠 Memory"
            leftValue: "<font color='" + root.usageColor + "'>"
                + Services.SystemStats.memPercent + "%</font>"
            leftLabel: "usage"
            rightValue: Services.SystemStats.memUsedGb + " GB"
            rightLabel: "of " + Services.SystemStats.memTotalGb + " GB"
            progress: Services.SystemStats.memPercent
            progressColor: root.usageColor

            entries: [
                ["Used", Services.SystemStats.memUsedGb + " GB"],
                ["Free", Services.SystemStats.memFreeGb + " GB"],
                ["Total", Services.SystemStats.memTotalGb + " GB"]
            ]
        }
    }
}
