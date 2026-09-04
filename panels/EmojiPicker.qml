import QtQuick
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// Emoji picker — search a vendored emoji table, then copy the character and
/// type it into whatever had focus. Replaces the rofi/fuzzel emoji scripts.
Components.OverlayPanel {
    id: picker

    onOpened: {
        emojiSearch.text = "";
        selectedIndex = 0;
    }
    onFocusRequested: emojiSearch.forceActiveFocus()

    property string fontFamily: Services.Settings.fontFamily
    property var allEmoji: []
    property int selectedIndex: 0

    readonly property int columns: 12
    readonly property int cellSize: 44

    // Name and tags both feed the match, so "grin" and "#face" work alike.
    property var filtered: Services.Fuzzy.filter(allEmoji, emojiSearch.text,
        e => e.name + " " + e.tags)

    onFilteredChanged: selectedIndex = 0

    readonly property var selected:
        selectedIndex >= 0 && selectedIndex < filtered.length ? filtered[selectedIndex] : null

    /// wtype is optional — probed once so a missing binary degrades to copy-only
    /// rather than leaving the pick silently doing nothing.
    property bool canType: false

    function pick() {
        if (!selected) return;
        let ch = selected.char;
        // argv positional, never interpolated: the table is data we did not author.
        copyProc.command = ["wl-copy", "--", ch];
        copyProc.running = true;
        close();
        if (canType) {
            typeProc.command = ["wtype", "--", ch];
            typeDelay.restart();
        }
    }

    // close() drops keyboard focus immediately but the surface stays mapped for
    // Ui.durClose. Typing before the compositor has refocused the previous window
    // sends the keystrokes into this dying overlay — the same race the old
    // emoji.sh papered over with `sleep 0.2`.
    Timer {
        id: typeDelay
        interval: Services.Ui.durClose + 60
        onTriggered: typeProc.running = true
    }

    Components.OverlaySurface {
        anchors.centerIn: parent
        width: Math.min(picker.columns * picker.cellSize + 32, parent.width - 32)
        height: Math.min(520, parent.height - 32)
        opacity: picker.contentOpacity

        Column {
            anchors.fill: parent

            Components.SearchHeader {
                icon: ""
                placeholder: "Search emoji..."
                showPlaceholder: emojiSearch.text.length === 0
                info: picker.filtered.length + " emoji"
                topLeftRadius: Services.Ui.radiusLg
                topRightRadius: Services.Ui.radiusLg

                TextInput {
                    id: emojiSearch
                    anchors.fill: parent
                    verticalAlignment: TextInput.AlignVCenter
                    color: Services.Colors.foreground
                    font.family: picker.fontFamily
                    font.pixelSize: 17
                    clip: true

                    function nav(dir) {
                        picker.selectedIndex = Services.Ui.gridStep(picker.selectedIndex,
                            picker.filtered.length, picker.columns, dir);
                        emojiGrid.positionViewAtIndex(picker.selectedIndex, GridView.Contain);
                    }

                    Keys.onEscapePressed: picker.close()
                    Keys.onReturnPressed: picker.pick()
                    Keys.onEnterPressed: picker.pick()
                    Keys.onUpPressed: nav("up")
                    Keys.onDownPressed: nav("down")
                    Keys.onLeftPressed: nav("left")
                    Keys.onRightPressed: nav("right")
                }
            }

            Rectangle {
                width: parent.width
                height: 2
                color: Services.Colors.color5
            }

            GridView {
                id: emojiGrid
                width: parent.width
                height: parent.height - 48 - 2 - 40
                leftMargin: 16
                rightMargin: 16
                topMargin: 8
                model: picker.filtered
                clip: true
                currentIndex: picker.selectedIndex
                cellWidth: picker.cellSize
                cellHeight: picker.cellSize
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    required property var modelData
                    required property int index

                    width: emojiGrid.cellWidth
                    height: emojiGrid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: Services.Ui.radiusSm
                        color: index === picker.selectedIndex
                            ? Services.Colors.accent
                            : (cellHover.containsMouse
                                ? Services.Colors.accentAlpha(0.25)
                                : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: modelData.char
                            textFormat: Text.PlainText
                            // Named outright, not via Settings.fontFamily: that is
                            // required to be a Nerd Font, which has no colour emoji.
                            // QML's font value type has no `families` fallback list
                            // (C++-only), and this cell holds nothing but the emoji.
                            font.family: "Noto Color Emoji"
                            font.pixelSize: 24
                        }

                        MouseArea {
                            id: cellHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: picker.selectedIndex = index
                            onClicked: {
                                picker.selectedIndex = index;
                                picker.pick();
                            }
                        }
                    }
                }
            }

            // ─── Selected emoji's name ───
            Rectangle {
                width: parent.width
                height: 40
                color: Services.Colors.color0
                bottomLeftRadius: Services.Ui.radiusLg
                bottomRightRadius: Services.Ui.radiusLg

                Components.StyledText {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: picker.selected
                        ? picker.selected.name + "   " + picker.selected.tags
                        : "No match"
                    color: Services.Colors.color8
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }
        }
    }

    // ─── Emoji table ─── vendored so the shell stays self-contained.
    FileView {
        path: Services.Paths.base + "/assets/emoji.txt"
        onLoaded: {
            let rows = [];
            for (const line of this.text().split("\n")) {
                if (!line) continue;
                // "<char>  <NAME>  #tag #tag" — the char never contains a space,
                // ZWJ sequences use U+200D.
                let parts = line.split("  ");
                if (parts.length < 2) continue;
                rows.push({
                    char: parts[0],
                    name: parts[1],
                    tags: parts[2] ?? ""
                });
            }
            picker.allEmoji = rows;
        }
        onLoadFailed: console.warn("EmojiPicker: could not read assets/emoji.txt")
    }

    Process {
        id: copyProc
        command: ["true"]
        running: false
    }

    Process {
        id: typeProc
        command: ["true"]
        running: false
    }

    Process {
        id: wtypeProbe
        running: true
        command: ["sh", "-c", "command -v wtype >/dev/null 2>&1 && echo y || echo n"]
        stdout: StdioCollector {
            onStreamFinished: picker.canType = this.text.trim() === "y"
        }
    }
}
