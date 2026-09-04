import QtQuick
import QtQuick.Layouts
import Quickshell.Services.UPower
import "../services" as Services
import "../components" as Components

/// Battery bar widget — native UPower, laptops only. Auto-hides when no laptop battery
/// is present (e.g. desktops). Click opens a detail popup (state, time, health).
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.batteryUnderline)

    readonly property var dev: UPower.displayDevice
    readonly property bool present: dev && dev.isPresent && dev.isLaptopBattery

    // UPower may report percentage as 0–1 or 0–100 depending on version; handle both.
    readonly property int pct: !dev ? 0
        : (dev.percentage <= 1.0 ? Math.round(dev.percentage * 100) : Math.round(dev.percentage))
    readonly property bool charging: dev
        && (dev.state === UPowerDeviceState.Charging || dev.state === UPowerDeviceState.PendingCharge)

    Layout.fillHeight: true
    visible: present
    implicitWidth: present ? body.implicitWidth : 0

    // Battery color is inverse of CPU/mem — low charge is the alarming end.
    function batteryColor(p, ch) {
        if (ch) return Services.Colors.green;
        if (p <= (instance.critPct ?? 15)) return Services.Colors.red;
        if (p <= (instance.lowPct ?? 30)) return Services.Colors.orange;
        if (p <= (instance.warnPct ?? 50)) return Services.Colors.yellow;
        return Services.Colors.green;
    }

    function formatTime(seconds) {
        let s = Math.floor(seconds);
        if (s <= 0) return "";
        let h = Math.floor(s / 3600);
        let m = Math.floor((s % 3600) / 60);
        if (h > 0) return h + "h " + m + "m";
        return m + "m";
    }

    Components.BarText {
        id: body
        bar: root.bar
        style: root.instance.style ?? "underline"
        accent: root.accent
        clickable: true
        onClicked: battPopup.isOpen ? battPopup.close() : battPopup.open()
        text: {
            let col = Services.Icons.hex(root.batteryColor(root.pct, root.charging));
            let time = (root.instance.showTime ?? false) && root.dev
                ? root.formatTime(root.charging ? root.dev.timeToFull : root.dev.timeToEmpty)
                : "";
            return Services.Icons.batteryPrefix(root.pct, root.charging, root.instance.iconSet ?? "emoji", root.accent)
                + "<font color='" + col + "'>" + root.pct + "%</font>"
                + (time === "" ? "" : " " + time);
        }
    }

    Components.WidgetPopup {
        id: battPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 300
        accentColor: Services.Colors.batteryUnderline

        Column {
            width: parent.width
            spacing: 12

            Components.PopupHeader {
                text: "🔋 Battery"
                separatorOpacity: 0.3
            }

            Components.StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                textFormat: Text.RichText
                font.pixelSize: 36
                font.bold: true
                text: "<font color='" + Services.Icons.hex(root.batteryColor(root.pct, root.charging)) + "'>"
                    + root.pct + "%</font>"
            }

            Components.ProgressBar {
                value: root.pct
                fillColor: root.batteryColor(root.pct, root.charging)
            }

            Components.Separator { }

            Components.StatRows {
                readonly property string timeLeft: root.dev
                    ? root.formatTime(root.charging ? root.dev.timeToFull : root.dev.timeToEmpty)
                    : ""

                entries: [
                    ["State", root.dev ? UPowerDeviceState.toString(root.dev.state) : "Unknown"]
                ].concat(timeLeft.length > 0
                    ? [[root.charging ? "Time to full" : "Time to empty", timeLeft]] : []
                ).concat(root.dev && root.dev.healthSupported
                    ? [["Health", Math.round(root.dev.healthPercentage) + "%"]] : []
                )
            }
        }
    }
}
