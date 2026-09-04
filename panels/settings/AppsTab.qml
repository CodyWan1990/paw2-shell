import QtQuick
import "../../services" as Services
import "../../components" as Components

/// Apps & notifications — default applications + notification behavior.
Components.SettingsPage {
    id: root
    anchors.fill: parent

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Default Apps"; showDivider: false }

        // Detected from installed desktop entries rather than typed: the entry
        // carries its own parsed argv, so nothing here has to guess how to
        // launch it. Only roles something consumes — the terminal for upgrade
        // spawns and the terminal keybind, the editor for the Edit buttons.
    Components.StyledDropdown {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Terminal"
        labelWidth: 110
        model: Services.DefaultApps.terminalNames
        currentValue: Services.DefaultApps.terminalName
        onSelected: v => Services.DefaultApps.setTerminalByName(v)
    }

    Components.StyledText {
        x: root.horizontalPadding; width: root.rowWidth
        wrapMode: Text.WordWrap
        text: "Choosing a terminal also retargets whichever keybind spawns one."
        color: Services.Colors.color8
        font.pixelSize: 12
    }

    Components.StyledDropdown {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Editor"
        labelWidth: 110
        model: Services.DefaultApps.editorNames
        currentValue: Services.DefaultApps.editorName
        onSelected: v => Services.DefaultApps.setEditorByName(v)
    }

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Notifications"; }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Max visible"; from: 1; to: 10; stepSize: 1
        value: Services.Settings.maxVisible
        onMoved: v => Services.Settings.maxVisible = Math.round(v)
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Auto-dismiss"; from: 0; to: 30; stepSize: 1; suffix: "s"
        value: Services.Settings.autoDismissSeconds
        onMoved: v => Services.Settings.autoDismissSeconds = Math.round(v)
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        label: "History limit"; from: 10; to: 200; stepSize: 10
        value: Services.Settings.historyLimit
        onMoved: v => Services.Settings.historyLimit = Math.round(v)
    }
}
