import QtQuick
import "../../services" as Services
import "../../components" as Components

/// Wallpaper picker view — extracted from AppLauncher. State (allWallpapers,
/// wpSelectedIndex, applying, filteredWallpapers, applyWallpaper, the list/apply
/// Processes) lives on the `launcher` coordinator; this is the UI bound to it.
/// Bare `launcher.*` references resolve to the `launcher` property below.
Column {
    id: root

    required property var launcher

    // Filtered list lives with its search input. Raw data + selection stay on the
    // coordinator; resetting selection on filter change writes launcher.wpSelectedIndex.
    readonly property var filtered: Services.Fuzzy.filter(launcher.allWallpapers,
        wpSearchInput.text, name => name)
    onFilteredChanged: launcher.wpSelectedIndex = 0

    function focusView() { wpSearchInput.forceActiveFocus(); }
    function reset() { wpSearchInput.text = ""; }

    Components.SearchHeader {
        icon: "\uf03e"
        placeholder: "Search wallpapers..."
        showPlaceholder: wpSearchInput.text.length === 0
        info: root.filtered.length + " wallpapers"
        border.color: Services.Colors.accent
        border.width: 1

        TextInput {
            id: wpSearchInput
            anchors.fill: parent
            verticalAlignment: TextInput.AlignVCenter
            color: Services.Colors.foreground
            font.family: launcher.fontFamily
            font.pixelSize: 17
            clip: true

            Keys.onEscapePressed: launcher.close()
            Keys.onReturnPressed: {
                if (launcher.wpSelectedIndex >= 0 && launcher.wpSelectedIndex < root.filtered.length)
                    launcher.applyWallpaper(root.filtered[launcher.wpSelectedIndex]);
            }
            Keys.onTabPressed: event => { event.accepted = true; launcher.cycleTab(1); }
            Keys.onBacktabPressed: event => { event.accepted = true; launcher.cycleTab(-1); }
            function navWp(dir) {
                let cols = Math.max(1, Math.floor(wallpaperGrid.width / wallpaperGrid.cellWidth));
                launcher.wpSelectedIndex = Services.Ui.gridStep(launcher.wpSelectedIndex,
                    root.filtered.length, cols, dir);
                wallpaperGrid.positionViewAtIndex(launcher.wpSelectedIndex, GridView.Contain);
            }
            Keys.onLeftPressed: navWp("left")
            Keys.onRightPressed: navWp("right")
            Keys.onUpPressed: navWp("up")
            Keys.onDownPressed: navWp("down")
        }
    }

    // Binds the next applied wallpaper to whichever theme is active, so themes
    // carry a wallpaper without one being baked into the shipped theme JSON.
    Item {
        id: bindRow
        width: parent.width
        height: visible ? 30 : 0
        visible: Services.Theme.currentSlug !== ""

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Components.ChipButton {
                symbol: Services.Settings.bindWallpaperToTheme ? "☑" : "☐"
                active: Services.Settings.bindWallpaperToTheme
                size: 14
                anchors.verticalCenter: parent.verticalCenter
                onClicked: Services.Settings.bindWallpaperToTheme =
                    !Services.Settings.bindWallpaperToTheme
            }

            Text {
                textFormat: Text.PlainText
                anchors.verticalCenter: parent.verticalCenter
                text: "Also set for current theme (" + Services.Theme.currentSlug + ")"
                color: Services.Settings.bindWallpaperToTheme
                    ? Services.Colors.foreground : Services.Colors.color8
                font.family: launcher.fontFamily
                font.pixelSize: 13

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Services.Settings.bindWallpaperToTheme =
                        !Services.Settings.bindWallpaperToTheme
                }
            }
        }
    }

    Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

    Rectangle {
        width: parent.width
        height: launcher.applying !== "" ? 36 : 0
        color: Services.Colors.color0
        visible: launcher.applying !== ""
        clip: true

        Text {
            textFormat: Text.PlainText
            anchors.centerIn: parent
            text: "Applying " + launcher.applying + "..."
            color: Services.Colors.accent
            font.family: launcher.fontFamily
            font.pixelSize: 14
            font.italic: true
        }

        Behavior on height {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // No wallpapers ship with paw, so a fresh install lands here with an empty
    // grid. Say where the directory is configured rather than leaving it
    // looking broken; the search-miss case gets its own wording.
    Components.StyledText {
        id: emptyState
        visible: root.filtered.length === 0
        x: 10
        width: parent.width - 20
        height: visible ? implicitHeight + 20 : 0
        topPadding: 14
        wrapMode: Text.WordWrap
        text: launcher.allWallpapers.length === 0
            ? "No images in " + launcher.wallpaperDir
              + "\nSet your wallpaper directory in Settings → General."
            : "No wallpapers match that search."
        color: Services.Colors.color8
        font.pixelSize: 13
        font.italic: true
    }

    GridView {
        id: wallpaperGrid
        width: parent.width
        height: parent.height - 50 - bindRow.height - emptyState.height
            - (launcher.applying !== "" ? 36 : 0)
        cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 220)))
        cellHeight: 170
        clip: true
        model: launcher.activeView === "wallpapers" ? root.filtered : []
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            required property string modelData
            required property int index
            width: wallpaperGrid.cellWidth
            height: wallpaperGrid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                property bool active: wpThumbHover.containsMouse || index === launcher.wpSelectedIndex
                color: active ? Services.Colors.accent : Services.Colors.color0
                border.color: modelData === launcher.applying
                    ? Services.Colors.accent
                    : (active ? Services.Colors.accent : Services.Colors.color8)
                border.width: (modelData === launcher.applying || active) ? 2 : 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    Image {
                        width: parent.width
                        height: parent.height - 24
                        source: "file://" + launcher.wallpaperDir + "/" + modelData
                        sourceSize.width: 300
                        sourceSize.height: 180
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true

                        Rectangle {
                            anchors.fill: parent
                            color: Services.Colors.color8
                            opacity: 0.3
                            visible: parent.status === Image.Loading
                        }
                    }

                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        text: modelData
                        color: parent.parent.active
                            ? Services.Colors.background
                            : Services.Colors.foreground
                        font.family: launcher.fontFamily
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                MouseArea {
                    id: wpThumbHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: launcher.applyWallpaper(modelData)
                    onContainsMouseChanged: if (containsMouse) launcher.wpSelectedIndex = index
                }
            }
        }
    }
}
