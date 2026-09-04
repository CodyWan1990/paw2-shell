import QtQuick
import "../services" as Services

/// Small icon-button used in the bar-layout editor chips; hover highlights to activeColor.
Text {
    textFormat: Text.PlainText
    id: root

    property string symbol: ""
    property color activeColor: Services.Colors.accent
    property bool active: false
    property int size: 12
    signal clicked()

    text: symbol
    font.pixelSize: size
    color: (active || ma.containsMouse) ? activeColor : Services.Colors.color8

    MouseArea {
        id: ma
        anchors.fill: parent
        anchors.margins: -3
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
