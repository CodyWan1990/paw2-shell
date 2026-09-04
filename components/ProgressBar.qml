import QtQuick
import "../services" as Services

/// Display-only animated progress bar — used in CPU/Memory monitor popups.
Item {
    id: root
    width: parent ? parent.width : 0
    height: 20

    property real value: 0
    property color fillColor: Services.Colors.color1

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Services.Colors.color0

        Rectangle {
            width: Math.max(parent.radius * 2, parent.width * (root.value / 100))
            height: parent.height
            radius: parent.radius
            color: root.fillColor

            Behavior on width {
                NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
            }
        }
    }
}
