import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// MPRIS media player widget — track info + prev/play/next controls; click track info to open player popup.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    Layout.fillHeight: true
    implicitWidth: playerRow.implicitWidth + 10
    visible: activePlayer !== null

    property var activePlayer: Services.Media.playingPlayer

    Components.BarItem {
        style: root.instance.style ?? "underline"
        accentColor: Services.Colors.byKey(root.instance.color, Services.Colors.mprisUnderline)

        RowLayout {
            id: playerRow
            anchors.centerIn: parent
            spacing: 6

            Components.MediaTransportControls {
                visible: root.instance.showControls ?? true
                player: root.activePlayer
                spacing: 6
                sideSize: root.bar.fontSize
                playSize: root.bar.fontSize
                pointerCursor: false
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.smallFontSize
                font.weight: Font.Bold
                color: Services.Colors.foreground
                font.italic: !(root.activePlayer?.isPlaying ?? true)
                elide: Text.ElideRight
                maximumLineCount: 1
                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: root.instance.maxWidth ?? 250
                textFormat: Text.RichText

                text: {
                    if (!root.activePlayer) return "";
                    let set = root.instance.iconSet ?? "emoji";
                    let acc = Services.Colors.byKey(root.instance.color, Services.Colors.mprisUnderline);
                    let icon = set === "none" ? ""
                        : set === "nerd" ? Services.Icons.markup("music", "nerd", acc)
                        : Services.Media.playerIcon(root.activePlayer.identity);
                    let title = root.activePlayer.trackTitle || "";
                    let artist = root.activePlayer.trackArtist || "";
                    // Placeholders expand to "" when the player reports nothing, so
                    // collapse the separators an empty field leaves behind.
                    let display = Services.Fmt.fill(root.instance.format ?? "{artist} - {title}", {
                        artist: artist,
                        title: title,
                        album: root.activePlayer.trackAlbum || "",
                        player: root.activePlayer.identity || ""
                    }).replace(/^[\s\-—:]+|[\s\-—:]+$/g, "");
                    let max = root.instance.maxChars ?? 35;
                    if (display.length > max) display = display.substring(0, Math.max(1, max - 3)) + "...";
                    return (icon === "" ? "" : icon + " ") + Services.Icons.esc(display);
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mprisPopup.isOpen ? mprisPopup.close() : mprisPopup.open()
                }
            }
        }
    }

    Components.WidgetPopup {
        id: mprisPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 360
        accentColor: Services.Colors.mprisUnderline

        // Inner FocusScope for Space key (Escape bubbles up to WidgetPopup's FocusScope)
        // Must use explicit sizing (not anchors.fill) so childrenRect auto-height works
        FocusScope {
            width: parent.width
            height: popupCol.implicitHeight
            focus: true
            Keys.onSpacePressed: root.activePlayer?.togglePlaying()

            Column {
                id: popupCol
                width: parent.width
                spacing: 10

                Components.PopupHeader {
                    text: "\u266B Now Playing"
                    showClose: true
                    onCloseClicked: mprisPopup.close()
                }

                Components.MediaTrackInfo {
                    width: parent.width
                    player: root.activePlayer
                    artSize: 72
                    titleSize: 16
                    artistSize: 14
                    textSpacing: 3
                }

                Components.MediaSeekBar {
                    width: parent.width
                    player: root.activePlayer
                    accentColor: Services.Colors.mprisUnderline
                    trackColor: Services.Colors.color0
                    trackHeight: 8
                }

                Components.MediaTransportControls {
                    anchors.horizontalCenter: parent.horizontalCenter
                    player: root.activePlayer
                    spacing: 28
                }

                Components.StyledText {
                    text: root.activePlayer?.identity ?? ""
                    color: Services.Colors.color8
                    font.pixelSize: 11
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
