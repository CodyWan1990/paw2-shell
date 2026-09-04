import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Artwork and plain-text MPRIS metadata.
RowLayout {
    id: root

    property var player: null
    property int artSize: 64
    property int titleSize: 15
    property int artistSize: 13
    property int albumSize: 12
    property int textSpacing: 2

    spacing: 12

    Image {
        source: root.player?.trackArtUrl ?? ""
        sourceSize.width: root.artSize
        sourceSize.height: root.artSize
        Layout.preferredWidth: root.artSize
        Layout.preferredHeight: root.artSize
        visible: status === Image.Ready
    }

    Column {
        Layout.fillWidth: true
        spacing: root.textSpacing

        StyledText {
            width: parent.width
            text: root.player?.trackTitle ?? ""
            color: Services.Colors.foreground
            font.pixelSize: root.titleSize
            font.bold: true
            elide: Text.ElideRight
        }
        StyledText {
            width: parent.width
            text: root.player?.trackArtist ?? ""
            color: Services.Colors.color8
            font.pixelSize: root.artistSize
            elide: Text.ElideRight
            visible: text !== ""
        }
        StyledText {
            width: parent.width
            text: root.player?.trackAlbum ?? ""
            color: Services.Colors.color8
            font.pixelSize: root.albumSize
            elide: Text.ElideRight
            visible: text !== ""
        }
    }
}
