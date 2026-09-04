import QtQuick
import Quickshell
import "../../services" as Services
import "../../components" as Components

/// App launcher view — extracted from AppLauncher. The filtered list lives here
/// with its search input; raw data (allApps), selection, grid sizing and the
/// launchApp action stay on the `launcher` coordinator (bare `launcher.*` refs
/// resolve to the property below).
Column {
    id: root

    required property var launcher
    property var contextApp: null

    readonly property var filtered: Services.Fuzzy.filter(launcher.allApps, appSearchInput.text,
        app => app.name + " " + (app.comment || "") + " " + (app.keywords || ""))
    onFilteredChanged: {
        launcher.appSelectedIndex = 0;
        appContext.visible = false;
    }
    onVisibleChanged: if (!visible) appContext.visible = false

    function focusView() { appSearchInput.forceActiveFocus(); }
    function reset() { appSearchInput.text = ""; }

    function selectedApp() {
        const index = launcher.appSelectedIndex;
        return index >= 0 && index < filtered.length ? filtered[index] : null;
    }

    function activateSelected(modifiers) {
        const app = selectedApp();
        if (!app)
            return;
        if (modifiers & Qt.ControlModifier)
            Services.Settings.toggleDockApp(app.id);
        else
            launcher.launchApp(app);
    }

    function openContext(item, app) {
        const pos = item.mapToItem(launcher.contentItem, 0, 0);
        contextApp = app;
        appContext.anchor.rect = Qt.rect(pos.x, pos.y, item.width, item.height);
        appContext.visible = true;
    }

    Connections {
        target: root.launcher
        function onIsOpenChanged() {
            if (!root.launcher.isOpen)
                appContext.visible = false;
        }
    }

    Components.SearchHeader {
        icon: "\uf002"
        placeholder: "Search applications..."
        showPlaceholder: appSearchInput.text.length === 0
        info: root.filtered.length + " apps · Ctrl+Enter pin"
        border.color: Services.Colors.accent
        border.width: 1

        TextInput {
            id: appSearchInput
            anchors.fill: parent
            verticalAlignment: TextInput.AlignVCenter
            color: Services.Colors.foreground
            font.family: launcher.fontFamily
            font.pixelSize: 17
            clip: true

            Keys.onEscapePressed: launcher.close()
            Keys.onReturnPressed: event => root.activateSelected(event.modifiers)
            Keys.onEnterPressed: event => root.activateSelected(event.modifiers)
            Keys.onTabPressed: event => { event.accepted = true; launcher.cycleTab(1); }
            Keys.onBacktabPressed: event => { event.accepted = true; launcher.cycleTab(-1); }
            function navApps(dir) {
                launcher.appSelectedIndex = Services.Ui.gridStep(launcher.appSelectedIndex,
                    root.filtered.length, launcher.appGridColumns, dir);
                appGrid.positionViewAtIndex(launcher.appSelectedIndex, GridView.Contain);
            }
            Keys.onUpPressed: navApps("up")
            Keys.onDownPressed: navApps("down")
            Keys.onLeftPressed: navApps("left")
            Keys.onRightPressed: navApps("right")
        }
    }

    Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

    GridView {
        id: appGrid
        width: parent.width
        height: parent.height - 50
        leftMargin: 16
        rightMargin: 16
        topMargin: 12
        model: launcher.activeView === "apps" ? root.filtered : []
        clip: true
        currentIndex: launcher.appSelectedIndex
        cellWidth: launcher.appCellWidth
        cellHeight: launcher.appCellHeight
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            required property var modelData
            required property int index

            width: appGrid.cellWidth
            height: appGrid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: 8
                color: index === launcher.appSelectedIndex
                    ? Services.Colors.accent
                    : (appHover.containsMouse
                        ? Services.Colors.accentAlpha(0.25)
                        : "transparent")

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Image {
                        id: appIcon
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: {
                            let ic = modelData.icon || "";
                            if (ic === "") return "";
                            if (ic.startsWith("/")) return "file://" + ic;
                            return Quickshell.iconPath(ic, true);  // "" if missing → letter fallback, no warning
                        }
                        sourceSize.width: 48
                        sourceSize.height: 48
                        width: 48
                        height: 48
                        visible: status === Image.Ready
                    }

                    Text {
                        textFormat: Text.PlainText
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.name ? modelData.name.charAt(0).toUpperCase() : "?"
                        font.pixelSize: 28
                        font.bold: true
                        color: index === launcher.appSelectedIndex
                            ? Services.Colors.background
                            : Services.Colors.color8
                        visible: appIcon.status !== Image.Ready
                        width: 48
                        height: 48
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        textFormat: Text.PlainText
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: appGrid.cellWidth - 20
                        text: modelData.name
                        color: index === launcher.appSelectedIndex
                            ? Services.Colors.background
                            : Services.Colors.foreground
                        font.family: launcher.fontFamily
                        font.pixelSize: 13
                        font.bold: index === launcher.appSelectedIndex
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }
                }

                MouseArea {
                    id: appHover
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: if (containsMouse) launcher.appSelectedIndex = index
                    onClicked: event => {
                        launcher.appSelectedIndex = index;
                        if (event.button === Qt.RightButton)
                            root.openContext(parent, root.filtered[index]);
                        else
                            launcher.launchApp(root.filtered[index]);
                    }
                }
            }
        }
    }

    PopupWindow {
        id: appContext
        visible: false
        grabFocus: true
        color: "transparent"
        implicitWidth: pinButton.implicitWidth + 16
        implicitHeight: pinButton.implicitHeight + 16
        // The filtered grid destroys delegates while searching. Copy the
        // coordinates, but keep the popup anchored to the stable launcher.
        anchor.window: root.launcher
        anchor.edges: Edges.Top | Edges.Left
        anchor.gravity: Edges.Bottom | Edges.Right
        anchor.adjustment: PopupAdjustment.Slide | PopupAdjustment.Flip

        FocusScope {
            anchors.fill: parent
            focus: true
            Keys.onEscapePressed: appContext.visible = false
            Keys.onReturnPressed: pinButton.keyActivate()
            Keys.onEnterPressed: pinButton.keyActivate()

            Rectangle {
                anchors.fill: parent
                radius: Services.Ui.radiusMd
                color: Services.Colors.background
                border.color: Services.Colors.accent
                border.width: 1

                Components.TextButton {
                    id: pinButton
                    anchors.centerIn: parent
                    text: Services.Settings.isDockAppPinned(root.contextApp?.id)
                        ? "Remove from Dock" : "Pin to Dock"
                    onClicked: {
                        if (root.contextApp)
                            Services.Settings.toggleDockApp(root.contextApp.id);
                        appContext.visible = false;
                    }
                }
            }
        }
    }
}
