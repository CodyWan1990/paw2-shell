import QtQuick
import "../services" as Services

/// Draggable slider with track, fill, and thumb — used in popup volume/brightness controls.
Item {
    id: root
    width: parent ? parent.width : 0
    height: 24

    property real value: 0
    property color accentColor: Services.Colors.color1
    property bool muted: false
    property int trackHeight: 6
    property color trackColor: Services.Colors.color0
    property bool showThumbOnHoverOnly: false

    signal moved(real value)

    property color _fillColor: muted ? Services.Colors.color8 : accentColor
    property color _thumbColor: muted ? Services.Colors.color8 : Services.Colors.foreground

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: root.trackHeight
        radius: height / 2
        color: root.trackColor

        Rectangle {
            width: parent.width * root.value
            height: parent.height
            radius: parent.radius
            color: root._fillColor
        }
    }

    Rectangle {
        x: (parent.width - width) * root.value
        anchors.verticalCenter: parent.verticalCenter
        width: 16; height: 16; radius: 8
        color: root._thumbColor
        visible: !root.showThumbOnHoverOnly
            || sliderArea.containsMouse
            || sliderArea.pressed
    }

    MouseArea {
        id: sliderArea
        anchors.fill: parent
        anchors.topMargin: -8
        anchors.bottomMargin: -8
        hoverEnabled: root.showThumbOnHoverOnly

        function update(mouseX) {
            root.moved(Math.max(0, Math.min(1.0, mouseX / track.width)));
        }

        onPressed: event => update(event.x)
        onPositionChanged: event => { if (pressed) update(event.x); }
    }
}
