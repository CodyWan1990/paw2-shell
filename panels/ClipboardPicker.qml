import QtQuick
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// Clipboard history picker — replaces cliphist + rofi dmenu.
Components.OverlayPanel {
    id: picker

    onOpened: {
        clipListProc.running = true;
        clipSearch.text = "";
        selectedIndex = 0;
    }
    onFocusRequested: clipSearch.forceActiveFocus()

    property string fontFamily: Services.Settings.fontFamily
    property var allEntries: []
    property int selectedIndex: 0
    property string scriptsDir: Services.Paths.scriptsDir

    property var filteredEntries: Services.Fuzzy.filter(allEntries, clipSearch.text, e => e.display)

    onFilteredEntriesChanged: selectedIndex = 0

    function selectEntry() {
        if (selectedIndex >= 0 && selectedIndex < filteredEntries.length) {
            let entry = filteredEntries[selectedIndex];
            pasteProc.command = ["bash", scriptsDir + "/clip-select.sh", entry.line];
            pasteProc.running = true;
            close();
        }
    }

    function deleteEntry(idx) {
        if (idx >= 0 && idx < filteredEntries.length) {
            let entry = filteredEntries[idx];
            clipDeleteProc.command = ["bash", scriptsDir + "/clip-delete.sh", entry.line];
            clipDeleteProc.running = true;
        }
    }

    function clearAll() {
        clipWipeProc.running = true;
    }

    Components.OverlaySurface {
        anchors.centerIn: parent
        width: Math.min(600, parent.width - 32)
        height: Math.min(500, parent.height - 32)
        opacity: picker.contentOpacity

        Column {
            anchors.fill: parent

            // ─── Header bar ───
            Components.SearchHeader {
                icon: "\uf0ea"
                placeholder: "Search clipboard..."
                showPlaceholder: clipSearch.text.length === 0
                info: picker.filteredEntries.length + " items"
                topLeftRadius: Services.Ui.radiusLg
                topRightRadius: Services.Ui.radiusLg

                TextInput {
                    id: clipSearch
                    anchors.fill: parent
                    verticalAlignment: TextInput.AlignVCenter
                    color: Services.Colors.foreground
                    font.family: picker.fontFamily
                    font.pixelSize: 17
                    clip: true

                    Keys.onEscapePressed: picker.close()
                    Keys.onReturnPressed: picker.selectEntry()
                    Keys.onUpPressed: {
                        if (picker.selectedIndex > 0) picker.selectedIndex--;
                        clipListView.positionViewAtIndex(picker.selectedIndex, ListView.Visible);
                    }
                    Keys.onDownPressed: {
                        if (picker.selectedIndex < picker.filteredEntries.length - 1) picker.selectedIndex++;
                        clipListView.positionViewAtIndex(picker.selectedIndex, ListView.Visible);
                    }
                    Keys.onDeletePressed: picker.deleteEntry(picker.selectedIndex)
                }

                trailing: Rectangle {
                    width: clearAllRow.width + 16
                    height: 28
                    radius: 4
                    color: clearAllMouse.containsMouse
                        ? Services.Colors.withAlpha(Services.Colors.color1, 0.8)
                        : Services.Colors.withAlpha(Services.Colors.color1, 0.4)
                    visible: picker.allEntries.length > 0

                    Row {
                        id: clearAllRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: "\uf00d"
                            color: Services.Colors.foreground
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "Clear"
                            color: Services.Colors.foreground
                            font.family: picker.fontFamily
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        id: clearAllMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: picker.clearAll()
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 2
                color: Services.Colors.color5
            }

            // ─── Clipboard entry list ───
            ListView {
                id: clipListView
                width: parent.width
                height: parent.height - 50
                model: picker.filteredEntries
                clip: true
                currentIndex: picker.selectedIndex

                delegate: Components.InteractiveListRow {
                    required property var modelData
                    required property int index

                    width: clipListView.width
                    height: 36
                    selected: index === picker.selectedIndex
                    clickRightMargin: 36
                    property bool rowHovered: hovered || deleteBtnMouse.containsMouse
                    onClicked: {
                        picker.selectedIndex = index;
                        picker.selectEntry();
                    }

                    // Clipboard contents are whatever was last copied — could be
                    // markup from a web page. StyledText is PlainText by default.
                    Components.StyledText {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: deleteBtn.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.display
                        color: index === picker.selectedIndex
                            ? Services.Colors.background
                            : Services.Colors.foreground
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Rectangle {
                        id: deleteBtn
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        radius: 12
                        color: deleteBtnMouse.containsMouse
                            ? Services.Colors.withAlpha(Services.Colors.color1, 0.7)
                            : "transparent"
                        visible: rowHovered || index === picker.selectedIndex

                        Text {
                            anchors.centerIn: parent
                            text: "\u2715"
                            color: index === picker.selectedIndex
                                ? Services.Colors.background
                                : Services.Colors.foreground
                            font.pixelSize: 12
                        }

                        MouseArea {
                            id: deleteBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: picker.deleteEntry(index)
                        }
                    }
                }
            }
        }
    }

    // ─── Load clipboard history ───
    Process {
        id: clipListProc
        command: ["cliphist", "list"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                let lines = this.text.trim().split("\n");
                let entries = [];
                for (let i = 0; i < lines.length; i++) {
                    let line = lines[i];
                    if (!line) continue;
                    let tabIdx = line.indexOf("\t");
                    entries.push({
                        line: line,
                        display: tabIdx >= 0 ? line.substring(tabIdx + 1) : line
                    });
                }
                picker.allEntries = entries;
                console.log("ClipboardPicker: loaded", entries.length, "entries");
            }
        }
    }

    // ─── Paste selected entry ───
    Process {
        id: pasteProc
        command: ["true"]
        running: false
    }

    // ─── Delete single entry ───
    Process {
        id: clipDeleteProc
        command: ["true"]
        running: false
        onRunningChanged: {
            if (!running) {
                clipListProc.running = true;
            }
        }
    }

    // ─── Wipe all entries ───
    Process {
        id: clipWipeProc
        command: ["cliphist", "wipe"]
        running: false
        onRunningChanged: {
            if (!running) {
                picker.allEntries = [];
            }
        }
    }
}
