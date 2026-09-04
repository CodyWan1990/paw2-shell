import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// Compact CPU + RAM display for the bar. Click opens a detail popup.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    Layout.fillHeight: true
    implicitWidth: row.implicitWidth

    Components.BarItem {
        style: root.instance.style ?? "underline"
        accentColor: Services.Colors.byKey(root.instance.color, Services.Colors.cpuUnderline)

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: cpuText.implicitWidth + 10
            Layout.topMargin: 1
            Layout.bottomMargin: 1

            Text {
                id: cpuText
                anchors.centerIn: parent
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.smallFontSize
                font.weight: Font.Bold
                textFormat: Text.RichText
                text: {
                    let usage = "<font color='" + Services.SystemStats.cpuColor + "'>"
                        + Services.SystemStats.cpuPercent.toFixed(0) + "%</font>";
                    let temp = (Services.SystemStats.cpuTemp > 0 && (root.instance.showTemp ?? true))
                        ? " <font color='" + Services.SystemStats.cpuTempColor + "'>"
                          + Services.SystemStats.cpuTemp + "\u00B0C</font>"
                        : "";
                    return Services.Icons.prefix("cpu", root.instance.iconSet ?? "emoji", Services.Colors.byKey(root.instance.color, Services.Colors.cpuUnderline))
                        + Services.Fmt.fill(root.instance.format ?? "{ghz}GHz ({pct}){temp}", {
                            ghz: Services.SystemStats.cpuFreqGhz,
                            pct: usage,
                            temp: temp,
                            cores: Services.SystemStats.cpuCores
                        });
                }
                color: Services.Colors.foreground
            }

            MouseArea {
                anchors.fill: parent
                onClicked: cpuPopup.isOpen ? cpuPopup.close() : cpuPopup.open()
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: memText.implicitWidth + 10
            Layout.topMargin: 1
            Layout.bottomMargin: 1

            Text {
                id: memText
                anchors.centerIn: parent
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.smallFontSize
                font.weight: Font.Bold
                textFormat: Text.RichText
                text: {
                    let pct = "<font color='" + Services.SystemStats.memColor + "'>"
                        + Services.SystemStats.memPercent + "%</font>";
                    return Services.Icons.prefix("memory", root.instance.iconSet ?? "emoji", Services.Colors.byKey(root.instance.color, Services.Colors.cpuUnderline))
                        + Services.Fmt.fill(root.instance.memFormat ?? "{used}/{total}GB ({pct})", {
                            used: Services.SystemStats.memUsedGb,
                            total: Services.SystemStats.memTotalGb,
                            free: Services.SystemStats.memFreeGb,
                            pct: pct
                        });
                }
                color: Services.Colors.foreground
            }

            MouseArea {
                anchors.fill: parent
                onClicked: cpuPopup.isOpen ? cpuPopup.close() : cpuPopup.open()
            }
        }
    }
    }

    Components.WidgetPopup {
        id: cpuPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 380
        accentColor: Services.Colors.cpuUnderline
        alignLeft: true
        xClamp: 0

        // Same StatPopup scaffold as CpuMonitor/MemoryMonitor, with both stats.
        Components.StatPopup {
            headerText: "System Monitor"
            leftValue: "<font color='" + Services.SystemStats.cpuColor + "'>"
                + Services.SystemStats.cpuPercent.toFixed(1) + "%</font>"
            leftLabel: "cpu"
            rightValue: "<font color='" + Services.SystemStats.memColor + "'>"
                + Services.SystemStats.memPercent + "%</font>"
            rightLabel: "memory"
            progress: Services.SystemStats.cpuPercent
            progressColor: Services.SystemStats.cpuColor

            entries: [
                ["Model", Services.SystemStats.cpuModel],
                ["Cores / Freq", Services.SystemStats.cpuCores + " / "
                    + Services.SystemStats.cpuFreqGhz + " GHz"]
            ].concat(Services.SystemStats.cpuTemp > 0
                ? [["Temperature", "<font color='" + Services.SystemStats.cpuTempColor
                    + "'>" + Services.SystemStats.cpuTemp + "\u00B0C</font>", true]]
                : []
            ).concat([
                ["Memory", Services.SystemStats.memUsedGb + " / "
                    + Services.SystemStats.memTotalGb + " GB ("
                    + Services.SystemStats.memFreeGb + " GB free)"]
            ])
        }
    }
}
