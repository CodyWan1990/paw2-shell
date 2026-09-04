import QtQuick
import QtQuick.Layouts
import Quickshell
import "../services" as Services
import "../components" as Components

/// User-defined command/exec widget (waybar-style custom module), driven by its instance config.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.foreground)

    Layout.fillHeight: true
    implicitWidth: customItem.implicitWidth

    property string output: ""

    readonly property string command: (instance && instance.command) ? instance.command : ""
    readonly property int intervalMs: (instance && instance.intervalMs)
        ? Math.max(500, instance.intervalMs) : 5000

    function firstLine(s) {
        if (!s) return "";
        let i = s.indexOf("\n");
        return (i >= 0 ? s.slice(0, i) : s).trim();
    }

    function displayText() {
        let icon = (instance && instance.icon) ? instance.icon + " " : "";
        let body = output !== "" ? output
            : ((instance && instance.label) ? instance.label : (command === "" ? "custom" : ""));
        return icon + body;
    }

    Components.BarText {
        id: customItem
        bar: root.bar
        style: root.instance.style ?? "underline"
        accent: root.accent
        clickable: !!(root.instance && root.instance.onClick)
        onClicked: if (root.instance && root.instance.onClick)
            Quickshell.execDetached(["sh", "-c", root.instance.onClick])
        // Output is arbitrary plain text — escape it for the RichText label.
        text: Services.Icons.esc(root.displayText())
    }

    Components.PolledProcess {
        command: ["sh", "-c", root.command]
        interval: root.intervalMs
        running: root.command !== ""
        onOutput: text => root.output = root.firstLine(text)
    }
}
