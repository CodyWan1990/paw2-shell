import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// Brightness widget — shows current brightness %. Click toggles slider popup.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.brightnessUnderline)
    Layout.fillHeight: true
    implicitWidth: brightItem.implicitWidth

    property int brightnessPercent: 100

    // Backend is swappable (brightnessctl, light, …) as long as it speaks
    // brightnessctl's -m/set interface.
    readonly property string backend: instance.command ?? "brightnessctl"
    readonly property int minPercent: instance.minPercent ?? 1

    function brightnessIcon() {
        let set = root.instance.iconSet ?? "emoji";
        let acc = root.accent;
        if (brightnessPercent < 25) return Services.Icons.prefix("bright-low", set, acc);
        return Services.Icons.prefix("bright-high", set, acc);
    }

    Components.BarText {
        id: brightItem
        bar: root.bar
        style: root.instance.style ?? "underline"
        accent: root.accent
        small: false
        clickable: true
        onClicked: brightPopup.isOpen ? brightPopup.close() : brightPopup.open()
        text: root.brightnessIcon() + root.brightnessPercent + "%"
    }

    Components.PolledProcess {
        id: brightPoll
        command: ["sh", "-c", "command -v " + root.backend + " >/dev/null 2>&1 && "
            + root.backend + " -m || echo 'none,,0,100%,0'"]
        interval: (root.instance.intervalSec ?? 5) * 1000
        onOutput: text => {
            try {
                // brightnessctl -m output: "device,class,current,percent%,max"
                let parts = text.trim().split(",");
                if (parts.length >= 4) {
                    let pct = parseInt(parts[3].replace("%", ""));
                    root.brightnessPercent = isNaN(pct) ? 100 : pct;
                }
            } catch (e) {
                console.warn("Brightness: parse error:", e);
            }
        }
    }

    Process {
        id: setProc
        property int targetPercent: 100
        command: [root.backend, "set", targetPercent + "%"]
        stdout: StdioCollector {
            onStreamFinished: brightPoll.poll()
        }
    }

    Components.WidgetPopup {
        id: brightPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 260
        contentHeight: 80
        contentMargins: 12
        accentColor: Services.Colors.brightnessUnderline
        xClamp: 0

        Column {
            width: parent.width
            spacing: 8

            Components.StyledText {
                text: "Brightness: " + root.brightnessPercent + "%"
                color: Services.Colors.foreground
                font.pixelSize: 14
                font.bold: true
            }

            Components.PopupSlider {
                value: root.brightnessPercent / 100
                accentColor: Services.Colors.brightnessUnderline
                onMoved: val => {
                    let pct = Math.round(Math.max(root.minPercent, Math.min(100, val * 100)));
                    root.brightnessPercent = pct;
                    setProc.targetPercent = pct;
                    setProc.running = true;
                }
            }
        }
    }
}
