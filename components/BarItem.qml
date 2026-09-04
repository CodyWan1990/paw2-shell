import QtQuick

/// Styled bar-item wrapper with configurable decoration: style none|underline|box|pill.
Item {
    id: barItem
    anchors.fill: parent
    anchors.topMargin: 1
    anchors.bottomMargin: 1

    property string style: "underline"
    property color underlineColor: "transparent"
    property color accentColor: underlineColor

    default property alias contentData: contentArea.data

    Rectangle {
        anchors.fill: parent
        visible: barItem.style === "box" || barItem.style === "pill"
        radius: barItem.style === "pill" ? height / 2 : 4
        color: barItem.style === "pill"
            ? Qt.rgba(barItem.accentColor.r, barItem.accentColor.g, barItem.accentColor.b, 0.25)
            : "transparent"
        border.color: barItem.style === "box" ? barItem.accentColor : "transparent"
        border.width: barItem.style === "box" ? 1 : 0
    }

    Item {
        id: contentArea
        anchors.fill: parent
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        color: barItem.accentColor
        visible: barItem.style === "underline"
    }
}
