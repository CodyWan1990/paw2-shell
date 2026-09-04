import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Shared chrome for the searchable panels' 48px header bar: leading icon,
/// an input slot, ghost placeholder, and trailing info text / extra actions.
/// The caller supplies its own TextInput (with its per-panel key handling) as
/// the default child — it fills the slot. Being a Rectangle, callers can set
/// border/radius directly.
Rectangle {
    id: root

    property string icon: ""
    property string placeholder: "Search..."
    property string info: ""
    property bool showPlaceholder: true
    default property alias inputSlot: inputHolder.data
    property alias trailing: trailingRow.data

    width: parent.width
    height: 48
    color: Services.Colors.color0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            textFormat: Text.PlainText
            text: root.icon
            font.pixelSize: 19
            color: Services.Colors.foreground
            Layout.alignment: Qt.AlignVCenter
        }

        Item {
            id: inputHolder
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        StyledText {
            text: root.info
            visible: text !== ""
            color: Services.Colors.color8
            font.pixelSize: 13
            Layout.alignment: Qt.AlignVCenter
        }

        Row {
            id: trailingRow
            spacing: 8
            Layout.alignment: Qt.AlignVCenter
        }
    }

    StyledText {
        anchors.left: parent.left
        anchors.leftMargin: 44
        anchors.verticalCenter: parent.verticalCenter
        text: root.placeholder
        color: Services.Colors.color8
        font.pixelSize: 17
        visible: root.showPlaceholder
    }
}
