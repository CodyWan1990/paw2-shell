import QtQuick
import "../services" as Services
import "../components" as Components

/// CPU bar widget — shows frequency, usage %, temp. Click opens detail popup.
Components.BarWidget {
    id: root

    icon: "cpu"
    defaultAccent: Services.Colors.cpuUnderline
    clickable: true
    onClicked: cpuPopup.isOpen ? cpuPopup.close() : cpuPopup.open()

    // Per-instance thresholds, so two CPU widgets can alarm at different points.
    readonly property color usageColor: Services.Colors.threshold(
        Services.SystemStats.cpuPercent,
        instance.warnPct ?? 50, instance.highPct ?? 65, instance.critPct ?? 80)

    text: {
        let usage = "<font color='" + root.usageColor + "'>"
            + Services.SystemStats.cpuPercent.toFixed(0) + "%</font>";
        let temp = (Services.SystemStats.cpuTemp > 0 && (instance.showTemp ?? true))
            ? " <font color='" + Services.SystemStats.cpuTempColor + "'>"
              + Services.SystemStats.cpuTemp + "°C</font>"
            : "";
        return Services.Fmt.fill(instance.format ?? "{ghz}GHz ({pct}){temp}", {
            ghz: Services.SystemStats.cpuFreqGhz,
            pct: usage,
            temp: temp,
            cores: Services.SystemStats.cpuCores
        });
    }

    Components.WidgetPopup {
        id: cpuPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 340
        accentColor: Services.Colors.cpuUnderline

        Components.StatPopup {
            headerText: "🖥️ CPU"
            leftValue: "<font color='" + root.usageColor + "'>"
                + Services.SystemStats.cpuPercent.toFixed(1) + "%</font>"
            leftLabel: "usage"
            rightVisible: Services.SystemStats.cpuTemp > 0
            rightValue: "<font color='" + Services.SystemStats.cpuTempColor + "'>"
                + Services.SystemStats.cpuTemp + "°C</font>"
            rightLabel: "temp"
            progress: Services.SystemStats.cpuPercent
            progressColor: root.usageColor

            entries: [
                ["Model", Services.SystemStats.cpuModel],
                ["Cores", Services.SystemStats.cpuCores],
                ["Frequency", Services.SystemStats.cpuFreqGhz + " GHz"]
            ]
        }
    }
}
