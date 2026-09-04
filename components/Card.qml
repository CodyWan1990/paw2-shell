import QtQuick
import "../services" as Services

/// Rounded surface card for grouping settings; optional title, children flow into a padded Column.
Rectangle {
    id: card
    width: parent ? parent.width : 0
    height: body.implicitHeight + 32
    radius: 10
    color: Services.Colors.color0
    border.color: Services.Colors.withAlpha(Services.Colors.color8, 0.4)
    border.width: 1

    property string title: ""
    default property alias content: inner.data

    Column {
        id: body
        x: 16
        y: 16
        width: card.width - 32
        spacing: 12

        StyledText {
            visible: card.title !== ""
            text: card.title
            color: Services.Colors.foreground
            font.pixelSize: 16
            font.bold: true
        }

        Column { id: inner; width: parent.width; spacing: 10 }
    }
}
