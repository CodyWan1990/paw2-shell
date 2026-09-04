import QtQuick
import "../services" as Services

/// Compact bordered text button used in panels, settings, and popups.
Rectangle {
    id: root

    property string text: ""
    property int fontSize: 13
    property bool fontBold: false
    property int horizontalPadding: 14
    property int buttonHeight: 32
    property color idleColor: Services.Colors.color0
    property color hoverColor: Services.Colors.accent
    property color idleTextColor: Services.Colors.foreground
    property color hoverTextColor: Services.Colors.background
    property color idleBorderColor: Services.Colors.color8
    property color hoverBorderColor: hoverColor
    property int borderWidth: 1
    /// Set by a host whose keyboard cursor is on this button.
    property bool cursor: false
    readonly property bool hovered: mouse.containsMouse

    signal clicked()

    /// Keyboard surface (see SettingsPage).
    function keyActivate() { if (enabled) clicked(); }

    implicitWidth: label.implicitWidth + horizontalPadding * 2
    implicitHeight: buttonHeight
    radius: 4
    color: hovered ? hoverColor : idleColor
    border.color: hovered ? hoverBorderColor
        : (cursor ? Services.Colors.accent : idleBorderColor)
    border.width: cursor ? 2 : borderWidth
    opacity: enabled ? 1 : 0.5

    StyledText {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.hovered ? root.hoverTextColor : root.idleTextColor
        font.pixelSize: root.fontSize
        font.bold: root.fontBold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
