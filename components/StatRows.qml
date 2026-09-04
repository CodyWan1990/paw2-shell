import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Two-column label/value detail grid, as used at the bottom of every monitor
/// popup. `rows` is a list of [label, value] pairs; an optional third element
/// marks the value as rich text, for the <font color> temperature values.
///
/// Rows that should only sometimes appear are composed in by the caller rather
/// than hidden here — a hidden cell still occupies its slot and would leave the
/// grid misaligned.
GridLayout {
    id: grid

    property var entries: []

    width: parent ? parent.width : 0
    columns: 2
    columnSpacing: 16
    rowSpacing: 8
    visible: entries.length > 0

    // Flattened to one cell per delegate: even index = label, odd = value.
    Repeater {
        model: grid.entries.reduce((acc, r) =>
            acc.concat([[r[0], false], [r[1], r[2] === true]]), [])

        delegate: StyledText {
            required property var modelData
            required property int index

            readonly property bool isLabel: index % 2 === 0
            text: modelData[0] === undefined ? "" : String(modelData[0])
            textFormat: modelData[1] ? Text.RichText : Text.PlainText
            color: isLabel ? Services.Colors.color8 : Services.Colors.foreground
            font.pixelSize: 14
            font.bold: !isLabel
            Layout.fillWidth: !isLabel
            wrapMode: Text.Wrap
        }
    }
}
