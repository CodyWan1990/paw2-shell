import QtQuick
import "../../components" as Components

/// Monitors — primary screen, resolution and refresh rate. The same component
/// the Welcome screen's first step uses, so changing a mode later doesn't mean
/// re-running setup.
Components.SettingsPage {
    id: root
    anchors.fill: parent

    Components.SectionHeader {
        x: root.horizontalPadding; width: root.rowWidth
        text: "Monitors"; showDivider: false
    }

    Components.MonitorSetup {
        x: root.horizontalPadding; width: root.rowWidth
    }
}
