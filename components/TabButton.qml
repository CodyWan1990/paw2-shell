import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Rounded tab button used by the launcher hub bar and the settings sub-tab bar.
Rectangle {
    id: tab

    property string text: ""
    property bool active: false
    property int hPad: 28
    /// Set when the host's keyboard cursor is parked on the tab row, so the
    /// tab that Left/Right would move away from is visible.
    property bool cursor: false
    signal clicked()

    Layout.preferredWidth: tabLabel.implicitWidth + hPad
    Layout.fillHeight: true
    Layout.topMargin: 6
    Layout.bottomMargin: 6
    radius: 6
    color: active
        ? Services.Colors.accent
        : (hover.containsMouse ? Services.Colors.accentAlpha(0.2) : "transparent")
    border.color: Services.Colors.accent
    border.width: cursor ? 2 : 0

    StyledText {
        id: tabLabel
        anchors.centerIn: parent
        text: tab.text
        color: tab.active ? Services.Colors.background : Services.Colors.foreground
        font.pixelSize: 14
        font.bold: tab.active
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tab.clicked()
    }
}
