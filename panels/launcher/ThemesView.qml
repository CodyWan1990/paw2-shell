import QtQuick
import QtQuick.Layouts
import "../../services" as Services
import "../../components" as Components

/// Theme switcher view — extracted from AppLauncher. Selection state
/// (themeSelectedIndex) and openCreateTheme live on the `launcher` coordinator;
/// the fuzzy-filtered grid model lives here with its search input.
FocusScope {
    id: root

    required property var launcher

    focus: launcher.activeView === "themes"
    function focusView() { themeSearch.forceActiveFocus(); }
    function reset() { themeSearch.text = ""; }

    // Fuzzy-filtered themes; the "+ New theme" tile only shows when not searching.
    readonly property var filteredThemes: Services.Fuzzy.filter(Services.Theme.themes,
        themeSearch.text, t => t.name)
    readonly property var gridModel: themeSearch.text === ""
        ? filteredThemes.concat([{ add: true }])
        : filteredThemes

    function activateSelected() {
        let m = gridModel;
        let idx = launcher.themeSelectedIndex;
        if (idx < 0 || idx >= m.length) return;
        if (m[idx].add === true) launcher.openCreateTheme();
        else Services.Theme.apply(m[idx].slug);   // stay open to preview
    }
    // Re-run the externals right away rather than waiting for the next theme
    // switch, so unticking one visibly removes what it wrote.
    function toggleKitty() {
        Services.Settings.themeKitty = !Services.Settings.themeKitty;
        Services.Theme.reapplyExternals();
    }
    function toggleGtk() {
        Services.Settings.themeGtk = !Services.Settings.themeGtk;
        Services.Theme.reapplyExternals();
    }

    function navThemes(dir) {
        let cols = Math.max(1, Math.floor((themeGrid.width - 32) / 200));
        launcher.themeSelectedIndex = Services.Ui.gridStep(launcher.themeSelectedIndex,
            gridModel.length, cols, dir);
        themeGrid.positionViewAtIndex(launcher.themeSelectedIndex, GridView.Contain);
    }

    Column {
        anchors.fill: parent

        Components.SearchHeader {
            icon: ""
            placeholder: "Search themes..."
            showPlaceholder: themeSearch.text.length === 0
            info: root.filteredThemes.length + " themes"
            border.color: Services.Colors.accent
            border.width: 1

            TextInput {
                id: themeSearch
                anchors.fill: parent
                verticalAlignment: TextInput.AlignVCenter
                color: Services.Colors.foreground
                font.family: launcher.fontFamily
                font.pixelSize: 17
                clip: true

                onTextChanged: launcher.themeSelectedIndex = 0
                Keys.onEscapePressed: launcher.creatingTheme
                    ? (launcher.creatingTheme = false) : launcher.close()
                Keys.onReturnPressed: root.activateSelected()
                Keys.onTabPressed: event => { event.accepted = true; launcher.cycleTab(1); }
                Keys.onBacktabPressed: event => { event.accepted = true; launcher.cycleTab(-1); }
                Keys.onLeftPressed: root.navThemes("left")
                Keys.onRightPressed: root.navThemes("right")
                Keys.onUpPressed: root.navThemes("up")
                Keys.onDownPressed: root.navThemes("down")
            }
        }

        Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

        GridView {
            id: themeGrid
            width: parent.width
            height: parent.height - 50 - externalsRow.height - 2
            leftMargin: 16
            rightMargin: 16
            topMargin: 12
            cellWidth: Math.floor((width - 32) / Math.max(1, Math.floor((width - 32) / 200)))
            cellHeight: 92
            clip: true
            currentIndex: launcher.themeSelectedIndex
            model: launcher.activeView === "themes" ? root.gridModel : []
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: themeCell
                required property var modelData
                required property int index
                readonly property bool isAdd: modelData.add === true
                width: themeGrid.cellWidth
                height: themeGrid.cellHeight

                Rectangle {
                    id: themeCard
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: 8
                    property bool current: !themeCell.isAdd && Services.Theme.currentSlug === themeCell.modelData.slug
                    property bool active: themeHover.containsMouse || current || themeCell.index === launcher.themeSelectedIndex
                    color: (active && !themeCell.isAdd)
                        ? Services.Colors.accent
                        : Services.Colors.color0
                    border.color: themeCell.isAdd
                        ? (active ? Services.Colors.accent : Services.Colors.color8)
                        : (current ? Services.Colors.foreground : Services.Colors.color8)
                    border.width: current ? 2 : 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        visible: themeCell.isAdd
                        Text {
                            text: "＋"
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: themeCard.active ? Services.Colors.accent : Services.Colors.color8
                            font.family: launcher.fontFamily
                            font.pixelSize: 26
                        }
                        Text {
                            text: "New theme"
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: themeCard.active ? Services.Colors.accent : Services.Colors.color8
                            font.family: launcher.fontFamily
                            font.pixelSize: 13
                        }
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        visible: !themeCell.isAdd

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            Text {
                                textFormat: Text.PlainText
                                text: themeCell.isAdd ? "" : themeCell.modelData.name
                                color: themeCard.active
                                    ? Services.Colors.background
                                    : Services.Colors.foreground
                                font.family: launcher.fontFamily
                                font.pixelSize: 15
                                font.bold: themeCard.current
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "✓"
                                visible: themeCard.current
                                color: Services.Colors.background
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }

                        Row {
                            spacing: 4
                            Repeater {
                                model: themeCell.isAdd ? [] : (themeCell.modelData.swatches || [])
                                delegate: Rectangle {
                                    required property string modelData
                                    width: 16; height: 16; radius: 3
                                    color: modelData
                                    border.width: 1
                                    border.color: Qt.rgba(0, 0, 0, 0.25)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: themeHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onContainsMouseChanged: if (containsMouse) launcher.themeSelectedIndex = themeCell.index
                        onClicked: {
                            launcher.themeSelectedIndex = themeCell.index;
                            if (themeCell.isAdd) launcher.openCreateTheme();
                            else Services.Theme.apply(themeCell.modelData.slug);   // stay open to preview
                        }
                    }
                }
            }
        }

        Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

        // What a theme switch touches outside paw2-shell's own config. Here rather
        // than in Settings because it is the kind of thing you want to flip while
        // picking a theme — same reasoning as the wallpaper binder in
        // WallpapersView. Mouse-only: the grid keeps the keyboard.
        Item {
            id: externalsRow
            width: parent.width
            height: 52

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Row {
                    spacing: 20

                    Row {
                        spacing: 8

                        Components.ChipButton {
                            symbol: Services.Settings.themeKitty ? "☑" : "☐"
                            active: Services.Settings.themeKitty
                            size: 14
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: root.toggleKitty()
                        }

                        Text {
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Theme kitty"
                            color: Services.Settings.themeKitty
                                ? Services.Colors.foreground : Services.Colors.color8
                            font.family: launcher.fontFamily
                            font.pixelSize: 13

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleKitty()
                            }
                        }
                    }

                    Row {
                        spacing: 8

                        Components.ChipButton {
                            symbol: Services.Settings.themeGtk ? "☑" : "☐"
                            active: Services.Settings.themeGtk
                            size: 14
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: root.toggleGtk()
                        }

                        Text {
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Theme GTK apps"
                            color: Services.Settings.themeGtk
                                ? Services.Colors.foreground : Services.Colors.color8
                            font.family: launcher.fontFamily
                            font.pixelSize: 13

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleGtk()
                            }
                        }
                    }
                }

                Text {
                    textFormat: Text.PlainText
                    text: "kitty gets a managed include in kitty.conf; GTK overwrites "
                        + "gtk.css. Both keep a .pawbak backup."
                    color: Services.Colors.color8
                    font.family: launcher.fontFamily
                    font.pixelSize: 11
                }
            }
        }
    }
}
