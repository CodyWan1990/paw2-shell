import QtQuick
import "../services" as Services
import "../components" as Components

/// Alt+Tab window switcher — replaces `rofi -show window`.
/// Lists open windows (Services.Paw.windows); the previously focused window is
/// pre-selected so a quick Alt+Tab → Enter swaps to the last window.
///
/// paw2 is the only source of the window list, so without it this never opens
/// (see the guard in shell.qml) rather than showing an empty panel.
Components.OverlayPanel {
    id: sw

    onOpened: {
        search.text = "";
        selectedIndex = (filteredEntries.length >= 2) ? 1 : 0;
    }
    onFocusRequested: search.forceActiveFocus()

    property string fontFamily: Services.Settings.fontFamily
    property int selectedIndex: 0

    property var filteredEntries: Services.Fuzzy.filter(Services.Paw.windows ?? [],
        search.text, w => (w.title ?? "") + " " + (w.appId ?? ""))

    // Empty search → pre-select the previous window (index 1); while filtering → top match.
    onFilteredEntriesChanged: selectedIndex = (search.text === "" && filteredEntries.length >= 2) ? 1 : 0

    function selectWindow() {
        if (selectedIndex >= 0 && selectedIndex < filteredEntries.length) {
            Services.Paw.focusWindow(filteredEntries[selectedIndex].id);
        }
        close();
    }

    function moveSelection(delta) {
        let n = filteredEntries.length;
        if (n === 0) return;
        selectedIndex = (selectedIndex + delta + n) % n;
        listView.positionViewAtIndex(selectedIndex, ListView.Contain);
    }

    Components.OverlaySurface {
        anchors.centerIn: parent
        width: Math.min(600, parent.width - 32)
        height: Math.min(500, parent.height - 32)
        opacity: sw.contentOpacity

        Column {
            anchors.fill: parent

            // ─── Header bar ───
            Components.SearchHeader {
                icon: "\uf2d2"
                placeholder: "Search windows..."
                showPlaceholder: search.text.length === 0
                info: sw.filteredEntries.length + " windows"
                topLeftRadius: Services.Ui.radiusLg
                topRightRadius: Services.Ui.radiusLg

                TextInput {
                    id: search
                    anchors.fill: parent
                    verticalAlignment: TextInput.AlignVCenter
                    color: Services.Colors.foreground
                    font.family: sw.fontFamily
                    font.pixelSize: 17
                    clip: true

                    Keys.onEscapePressed: sw.close()
                    Keys.onReturnPressed: sw.selectWindow()
                    Keys.onTabPressed: sw.moveSelection(1)
                    Keys.onBacktabPressed: sw.moveSelection(-1)
                    Keys.onUpPressed: sw.moveSelection(-1)
                    Keys.onDownPressed: sw.moveSelection(1)
                }
            }

            Rectangle {
                width: parent.width
                height: 2
                color: Services.Colors.color5
            }

            // ─── Window list ───
            ListView {
                id: listView
                width: parent.width
                height: parent.height - 50
                model: sw.filteredEntries
                clip: true
                currentIndex: sw.selectedIndex

                delegate: Components.InteractiveListRow {
                    required property var modelData
                    required property int index

                    width: listView.width
                    height: 44
                    selected: index === sw.selectedIndex
                    onPointerMoved: sw.selectedIndex = index
                    onClicked: {
                        sw.selectedIndex = index;
                        sw.selectWindow();
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        // Titles and app IDs are set by the window itself, so they go
                        // through StyledText, which is PlainText by default.
                        Components.StyledText {
                            width: parent.width
                            text: modelData.title && modelData.title.length > 0
                                ? modelData.title : "(untitled)"
                            color: index === sw.selectedIndex
                                ? Services.Colors.background
                                : Services.Colors.foreground
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        Components.StyledText {
                            width: parent.width
                            text: modelData.appId ?? ""
                            visible: text.length > 0
                            color: index === sw.selectedIndex
                                ? Services.Colors.background
                                : Services.Colors.color8
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }
                }
            }
        }
    }
}
