import QtQuick
import Quickshell
import "../services" as Services
import "../components" as Components

/// Per-monitor launcher and task dock.
PanelWindow {
    id: dock

    property string outputName: ""
    property int iconSize: Services.Settings.dockIconSize
    property string position: Services.Settings.dockPosition
    readonly property bool horizontal: position === "bottom"
    readonly property bool autoHide: Services.Settings.dockMode === "autohide"
    readonly property int edgeMargin: Services.Settings.dockEdgeMargin
    readonly property int pillThickness: iconSize + 16
    readonly property int topInset: Services.Settings.barPosition === "top"
        ? Services.Settings.barHeight + (Services.Settings.barFloating ? Services.Settings.barGap : 0)
        : 0
    readonly property int bottomInset: Services.Settings.barPosition === "bottom"
        ? Services.Settings.barHeight + (Services.Settings.barFloating ? Services.Settings.barGap : 0)
        : 0

    // A magnified icon grows out of the pill, so the window carries a headroom
    // strip on the inner side for it to grow into — without it the compositor
    // clips every icon the moment it lifts.
    readonly property real zoom: Services.Settings.dockMagnificationEnabled
        ? Services.Settings.dockMagnification : 1.0
    readonly property int zoomHeadroom: Math.ceil(iconSize * (zoom - 1))
    readonly property bool shadowActive: Services.Settings.dockShadowEnabled
        && Services.Settings.dockShadowRadius > 0
        && Services.Settings.dockShadowOpacity > 0
    readonly property int shadowPadding: shadowActive ? Math.ceil(
        Services.Settings.dockShadowRadius + Math.max(
            Math.abs(Services.Settings.dockShadowOffsetX),
            Math.abs(Services.Settings.dockShadowOffsetY))) : 0
    readonly property int drawingHeadroom: Math.max(zoomHeadroom, shadowPadding)
    // One slot per icon, wide enough for the icon at full zoom, so neighbours
    // never overlap and nothing in the layout moves while the pointer sweeps.
    readonly property int slotSize: Math.round(iconSize * zoom) + 6
    readonly property int windowThickness: pillThickness + edgeMargin + drawingHeadroom

    // The hover areas cover the pill's whole thickness and reach into the headroom
    // with the icons they belong to. Any band the slots do not cover is a band
    // where the pointer belongs to no icon and the magnification blinks off.
    readonly property int pillPadding: (pillThickness - iconSize) / 2
    readonly property int reachInner: -(zoomHeadroom + pillPadding)
    readonly property int reachTop: horizontal ? reachInner : 0
    readonly property int reachBottom: horizontal ? -pillPadding : 0
    readonly property int reachLeft: horizontal ? 0
        : (position === "right" ? reachInner : -pillPadding)
    readonly property int reachRight: horizontal ? 0
        : (position === "left" ? reachInner : -pillPadding)

    property bool shown: true
    property Item tooltipItem: null
    property string tooltipText: ""
    property var contextApp: null
    property string draggingEntryId: ""
    /// Pointer position along the dock, in dockGrid coordinates. -1 = off the dock.
    property real hoverAxis: -1

    readonly property var appItems: Services.Applications.buildDockItems(
        Services.Applications.entries, Services.Paw.windows,
        Services.Settings.dockPinnedApps, outputName,
        Services.Settings.dockShowRunningApps)

    anchors.left: horizontal || position === "left"
    anchors.right: horizontal || position === "right"
    anchors.top: !horizontal
    anchors.bottom: true
    margins.top: horizontal ? 0 : topInset
    margins.bottom: bottomInset
    implicitWidth: horizontal ? 0 : windowThickness
    implicitHeight: horizontal ? windowThickness : 0
    // The headroom is drawing space, not reserved space — windows tile up to the
    // pill, exactly as they did before magnification had room.
    exclusiveZone: visible && !autoHide ? pillThickness + edgeMargin : 0
    visible: Services.Settings.ready && Services.Settings.dockEnabled
        && (Services.Settings.dockShowLauncher || appItems.length > 0)
    color: "transparent"
    mask: Region { item: dock.shown ? interactionArea : revealArea }

    function iconPath(icon) {
        if (!icon)
            return "";
        return icon.startsWith("/") ? "file://" + icon : Quickshell.iconPath(icon, true);
    }

    function reveal() {
        hideTimer.stop();
        shown = true;
    }

    function scheduleHide() {
        if (!autoHide || dockHover.hovered || pillHover.hovered || revealHover.hovered
                || contextMenu.visible || draggingEntryId !== "")
            return;
        hideTimer.restart();
    }

    /// Remember where the pointer is along the dock. Every slot reports it, and
    /// the slots tile the dock, so exactly one of them owns the pointer at a time.
    function trackHover(item, x, y) {
        hoverClear.stop();
        const point = item.mapToItem(dockGrid, x, y);
        hoverAxis = horizontal ? point.x : point.y;
    }

    /// Cosine falloff around the pointer, measured against fixed slot centers so
    /// the scale can never feed back into the layout that produced it.
    function iconScale(slotCenter) {
        if (hoverAxis < 0 || !Services.Settings.dockMagnificationEnabled)
            return 1.0;
        const influence = iconSize * 2;
        const distance = Math.abs(hoverAxis - slotCenter);
        if (distance >= influence)
            return 1.0;
        const amount = (Math.cos(Math.PI * distance / influence) + 1) / 2;
        return 1 + (Services.Settings.dockMagnification - 1) * amount;
    }

    function showTooltip(item, text) {
        tooltipItem = item;
        tooltipText = text;
        tooltipDelay.restart();
    }

    function hideTooltip(item) {
        if (tooltipItem !== item)
            return;
        tooltipDelay.stop();
        tooltip.visible = false;
        tooltipItem = null;
    }

    function placePopup(popup, item) {
        if (!item)
            return false;
        const pos = item.mapToItem(dock.contentItem, 0, 0);
        popup.anchor.rect = Qt.rect(pos.x, pos.y, item.width, item.height);
        return true;
    }

    function activate(app) {
        const window = Services.Applications.nextWindow(app.windows);
        if (window)
            Services.Paw.focusWindow(window.id);
        else if (app.entry)
            app.entry.desktop.execute();
    }

    function launchNew(app) {
        if (app.entry)
            app.entry.desktop.execute();
    }

    function openContext(item, app) {
        // An unmatched running app cannot be pinned safely, but a stale pin
        // must still be removable after its desktop entry is uninstalled.
        if (!app.entry && !app.pinned)
            return;
        hideTooltip(item);
        contextApp = app;
        reveal();
        if (placePopup(contextMenu, item))
            contextMenu.visible = true;
    }

    function beginDrag(item, app) {
        hideTooltip(item);
        draggingEntryId = app.entryId;
        dragGhost.icon = app.icon;
        dragGhost.label = app.name;
        dragGhost.Drag.active = true;
    }

    function finishDrag(cancelled) {
        if (draggingEntryId === "")
            return;
        if (cancelled)
            dragGhost.Drag.cancel();
        else
            dragGhost.Drag.drop();
        dragGhost.Drag.active = false;
        draggingEntryId = "";
        scheduleHide();
    }

    onAutoHideChanged: {
        hideTimer.stop();
        shown = !autoHide;
    }
    onVisibleChanged: if (!visible) shown = !autoHide
    onShownChanged: if (!shown) {
        tooltipDelay.stop();
        tooltip.visible = false;
        tooltipItem = null;
        hoverAxis = -1;
    }
    onAppItemsChanged: {
        tooltip.visible = false;
        tooltipItem = null;
        contextMenu.visible = false;
    }

    Item {
        id: revealArea
        x: dock.position === "right" ? dock.width - width : 0
        y: dock.horizontal ? dock.height - height : 0
        width: dock.horizontal ? dock.width : 2
        height: dock.horizontal ? 2 : dock.height

        HoverHandler {
            id: revealHover
            onHoveredChanged: if (hovered) dock.reveal()
        }
    }

    Item {
        id: interactionArea
        x: dock.horizontal ? pill.x
            : (dock.position === "right" ? pill.x - dock.zoomHeadroom : 0)
        y: dock.horizontal ? pill.y - dock.zoomHeadroom : pill.y
        width: dock.horizontal ? pill.width
            : dock.pillThickness + dock.edgeMargin + dock.zoomHeadroom
        height: dock.horizontal
            ? dock.pillThickness + dock.edgeMargin + dock.zoomHeadroom : pill.height

        HoverHandler {
            id: dockHover
            onHoveredChanged: {
                if (hovered)
                    dock.reveal();
                else
                    dock.scheduleHide();
            }
        }
    }

    Item {
        id: pill
        x: dock.horizontal ? (dock.width - width) / 2
            : (dock.position === "left" ? dock.edgeMargin : dock.drawingHeadroom)
        y: dock.horizontal ? (dock.shown ? dock.drawingHeadroom : dock.height)
            : (dock.height - height) / 2
        // The slots tile the pill end to end: no padding along the dock axis
        // means no band where the pointer belongs to no icon at all.
        width: dock.horizontal ? dockGrid.implicitWidth : dock.pillThickness
        height: dock.horizontal ? dock.pillThickness : dockGrid.implicitHeight
        readonly property real cornerRadius: Math.min(
            Services.Settings.dockRadius, Math.min(width, height) / 2)

        Components.SurfaceShadow {
            anchors.fill: parent
            radius: pill.cornerRadius
            blur: Services.Settings.dockShadowRadius
            colorValue: Services.Settings.dockShadowColor
            colorOpacity: Services.Settings.dockShadowOpacity
            offset: Qt.vector2d(Services.Settings.dockShadowOffsetX,
                Services.Settings.dockShadowOffsetY)
            visible: dock.shadowActive
            z: -2
        }

        Rectangle {
            anchors.fill: parent
            radius: pill.cornerRadius
            color: Services.Colors.withAlpha(
                Services.Colors.background, Services.Settings.dockOpacity)
            border.color: Services.Colors.color8
            border.width: Services.Settings.dockBorderEnabled ? 1 : 0
            z: -1
        }

        HoverHandler {
            id: pillHover
            onHoveredChanged: {
                if (hovered)
                    dock.reveal();
                else
                    dock.scheduleHide();
            }
        }

        transform: Translate {
            x: !dock.horizontal && !dock.shown
                ? (dock.position === "left" ? -dock.windowThickness : dock.windowThickness)
                : 0

            Behavior on x {
                NumberAnimation {
                    duration: dock.shown ? Services.Ui.durOpen : Services.Ui.durClose
                    easing.type: Easing.OutCubic
                }
            }
        }

        Behavior on y {
            NumberAnimation {
                duration: dock.shown ? Services.Ui.durOpen : Services.Ui.durClose
                easing.type: Easing.OutCubic
            }
        }

        Grid {
            id: dockGrid
            anchors.centerIn: parent
            flow: dock.horizontal ? Grid.LeftToRight : Grid.TopToBottom
            rows: dock.horizontal ? 1 : -1
            columns: dock.horizontal ? -1 : 1
            spacing: 0

            Item {
                id: launcherItem
                width: dock.horizontal ? dock.slotSize : dock.iconSize
                height: dock.horizontal ? dock.iconSize : dock.slotSize
                visible: Services.Settings.dockShowLauncher

                Item {
                    anchors.centerIn: parent
                    width: dock.iconSize
                    height: dock.iconSize
                    scale: dock.iconScale(dock.horizontal
                        ? launcherItem.x + launcherItem.width / 2
                        : launcherItem.y + launcherItem.height / 2)
                    transformOrigin: dock.horizontal ? Item.Bottom
                        : (dock.position === "left" ? Item.Left : Item.Right)

                    Rectangle {
                        anchors.fill: parent
                        radius: Services.Ui.radiusMd
                        color: launcherMouse.containsMouse
                            ? Services.Colors.accentAlpha(0.25) : "transparent"
                    }

                    Components.StyledText {
                        anchors.centerIn: parent
                        text: "\uf00a"
                        color: Services.Colors.accent
                        font.family: Services.Settings.fontFamily
                        font.pixelSize: Math.round(dock.iconSize * 0.58)
                    }
                }

                MouseArea {
                    id: launcherMouse
                    anchors.fill: parent
                    anchors.topMargin: dock.reachTop
                    anchors.bottomMargin: dock.reachBottom
                    anchors.leftMargin: dock.reachLeft
                    anchors.rightMargin: dock.reachRight
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: {
                        dock.trackHover(launcherMouse, mouseX, mouseY);
                        dock.showTooltip(launcherItem, "Applications");
                    }
                    onExited: {
                        hoverClear.restart();
                        dock.hideTooltip(launcherItem);
                    }
                    onPositionChanged: mouse => dock.trackHover(launcherMouse, mouse.x, mouse.y)
                    onPressed: contextMenu.visible = false
                    onClicked: Services.Actions.launcher()
                }
            }

            Item {
                id: separatorItem
                width: dock.horizontal ? 12 : dock.iconSize
                height: dock.horizontal ? dock.iconSize : 12
                visible: Services.Settings.dockShowLauncher && dock.appItems.length > 0

                Rectangle {
                    anchors.centerIn: parent
                    width: dock.horizontal ? 1 : dock.iconSize - 12
                    height: dock.horizontal ? dock.iconSize - 12 : 1
                    color: Services.Colors.color8
                }

                // Inert, but it keeps the strip gapless: a slot that reported no
                // pointer would blink the magnification off as you crossed it.
                MouseArea {
                    id: separatorMouse
                    anchors.fill: parent
                    anchors.topMargin: dock.reachTop
                    anchors.bottomMargin: dock.reachBottom
                    anchors.leftMargin: dock.reachLeft
                    anchors.rightMargin: dock.reachRight
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onEntered: dock.trackHover(separatorMouse, mouseX, mouseY)
                    onExited: hoverClear.restart()
                    onPositionChanged: mouse => dock.trackHover(separatorMouse, mouse.x, mouse.y)
                }
            }

            Repeater {
                model: dock.appItems

                delegate: Item {
                    id: appItem
                    required property var modelData
                    required property int index
                    width: dock.horizontal ? dock.slotSize : dock.iconSize
                    height: dock.horizontal ? dock.iconSize : dock.slotSize
                    opacity: dock.draggingEntryId === modelData.entryId ? 0.4 : 1.0

                    Item {
                        id: appArtwork
                        anchors.centerIn: parent
                        width: dock.iconSize
                        height: dock.iconSize
                        scale: dock.iconScale(dock.horizontal
                            ? appItem.x + appItem.width / 2
                            : appItem.y + appItem.height / 2)
                        transformOrigin: dock.horizontal ? Item.Bottom
                            : (dock.position === "left" ? Item.Left : Item.Right)

                        Rectangle {
                            anchors.fill: parent
                            radius: Services.Ui.radiusMd
                            color: appMouse.containsMouse
                                ? Services.Colors.accentAlpha(0.25) : "transparent"
                        }

                        Image {
                            id: appIcon
                            anchors.centerIn: parent
                            width: dock.iconSize - 8
                            height: dock.iconSize - 8
                            source: dock.iconPath(appItem.modelData.icon)
                            sourceSize.width: width
                            sourceSize.height: height
                            visible: status === Image.Ready
                        }

                        Components.StyledText {
                            anchors.centerIn: parent
                            width: dock.iconSize - 8
                            height: dock.iconSize - 8
                            text: appItem.modelData.name
                                ? appItem.modelData.name.charAt(0).toUpperCase() : "?"
                            color: Services.Colors.foreground
                            font.pixelSize: Math.round(dock.iconSize * 0.52)
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            visible: appIcon.status !== Image.Ready
                        }
                    }

                    Grid {
                        readonly property int dotCount: Math.min(3, appItem.modelData.windows.length)
                        columns: dock.horizontal ? dotCount : 1
                        rows: dock.horizontal ? 1 : dotCount
                        spacing: 2
                        visible: Services.Settings.dockShowIndicators && dotCount > 0
                        x: dock.horizontal ? (parent.width - width) / 2
                            : (dock.position === "left" ? parent.width - width - 2 : 2)
                        y: dock.horizontal ? parent.height - height - 2 : (parent.height - height) / 2

                        Repeater {
                            model: parent.dotCount

                            Rectangle {
                                required property int index
                                width: dock.horizontal ? 5 : 3
                                height: dock.horizontal ? 3 : 5
                                radius: 2
                                color: appItem.modelData.windows.some(window => window.focused)
                                    ? Services.Colors.accent : Services.Colors.foreground
                            }
                        }
                    }

                    DropArea {
                        id: pinDrop
                        anchors.fill: parent
                        enabled: appItem.modelData.pinned
                        keys: ["dockpin"]
                        property bool after: false
                        onPositionChanged: drag => after = dock.horizontal
                            ? drag.x >= width / 2 : drag.y >= height / 2
                        onDropped: {
                            const id = dock.draggingEntryId;
                            const target = appItem.index + (after ? 1 : 0);
                            Qt.callLater(() => Services.Settings.moveDockApp(id, target));
                        }

                        Rectangle {
                            width: dock.horizontal ? 2 : parent.width
                            height: dock.horizontal ? parent.height : 2
                            x: dock.horizontal && parent.after ? parent.width - width : 0
                            y: !dock.horizontal && parent.after ? parent.height - height : 0
                            color: Services.Colors.accent
                            visible: parent.containsDrag
                        }
                    }

                    MouseArea {
                        id: appMouse
                        anchors.fill: parent
                        anchors.topMargin: dock.reachTop
                        anchors.bottomMargin: dock.reachBottom
                        anchors.leftMargin: dock.reachLeft
                        anchors.rightMargin: dock.reachRight
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                        hoverEnabled: true
                        cursorShape: dock.draggingEntryId === appItem.modelData.entryId
                            ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                        // Reordering starts once the pointer has actually travelled;
                        // arming it on press turned every click into a drag.
                        drag.target: appItem.modelData.pinned ? dragGhost : null
                        drag.threshold: 8
                        drag.onActiveChanged: if (appMouse.drag.active) {
                            appMouse.dragged = true;
                            dock.beginDrag(appItem, appItem.modelData);
                        }
                        /// The drag is over by the time onClicked runs, so the click
                        /// that ended it needs its own memory.
                        property bool dragged: false

                        onEntered: {
                            dock.trackHover(appMouse, mouseX, mouseY);
                            dock.showTooltip(appItem, appItem.modelData.name);
                        }
                        onExited: {
                            hoverClear.restart();
                            dock.hideTooltip(appItem);
                        }
                        onPositionChanged: mouse => dock.trackHover(appMouse, mouse.x, mouse.y)
                        onPressed: mouse => {
                            // The compositor's popup grab only dismisses on a click
                            // from another client, and the shell is one client.
                            contextMenu.visible = false;
                            dragged = false;
                            // Seed the ghost under the cursor: drag.target moves it
                            // from wherever it stands when the threshold is crossed.
                            const pos = appMouse.mapToItem(dock.contentItem, mouse.x, mouse.y);
                            dragGhost.x = pos.x - dragGhost.width / 2;
                            dragGhost.y = pos.y - dragGhost.height / 2;
                            if (mouse.button === Qt.RightButton)
                                dock.openContext(appItem, appItem.modelData);
                        }
                        onReleased: dock.finishDrag(false)
                        onCanceled: dock.finishDrag(true)
                        onClicked: event => {
                            if (appMouse.dragged || event.button === Qt.RightButton)
                                return;
                            if (event.button === Qt.MiddleButton)
                                dock.launchNew(appItem.modelData);
                            else
                                dock.activate(appItem.modelData);
                        }
                    }
                }
            }
        }
    }

    Item {
        id: dragGhost
        width: dock.iconSize
        height: dock.iconSize
        z: 1000
        visible: Drag.active
        property string icon: ""
        property string label: ""
        Drag.active: false
        Drag.keys: ["dockpin"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        Rectangle {
            anchors.fill: parent
            radius: Services.Ui.radiusMd
            color: Services.Colors.withAlpha(Services.Colors.background, 0.92)
            border.color: Services.Colors.accent
            border.width: 1
        }
        Image {
            anchors.centerIn: parent
            width: parent.width - 8
            height: parent.height - 8
            source: dock.iconPath(dragGhost.icon)
            sourceSize.width: width
            sourceSize.height: height
            visible: status === Image.Ready
        }
    }

    // Crossing from one slot to the next emits exited before entered; without the
    // deferral the magnification would blink off between every pair of icons.
    Timer {
        id: hoverClear
        interval: 0
        onTriggered: dock.hoverAxis = -1
    }

    Timer {
        id: hideTimer
        interval: Services.Settings.dockHideDelayMs
        onTriggered: {
            if (dock.autoHide && !dockHover.hovered && !pillHover.hovered
                    && !contextMenu.visible
                    && dock.draggingEntryId === "")
                dock.shown = false;
        }
    }

    Timer {
        id: tooltipDelay
        interval: 450
        onTriggered: {
            if (dock.placePopup(tooltip, dock.tooltipItem))
                tooltip.visible = true;
        }
    }

    PopupWindow {
        id: tooltip
        visible: false
        color: "transparent"
        implicitWidth: tooltipLabel.implicitWidth + 20
        implicitHeight: 30
        // Keep the anchor on the stable window. Quickshell 0.3 can crash when
        // a PopupAnchor follows a reactive delegate through destruction.
        anchor.window: dock
        anchor.edges: dock.horizontal ? Edges.Top | Edges.Left
            : (dock.position === "left" ? Edges.Top | Edges.Right : Edges.Top | Edges.Left)
        anchor.gravity: dock.horizontal ? Edges.Top | Edges.Right
            : (dock.position === "left" ? Edges.Bottom | Edges.Right : Edges.Bottom | Edges.Left)
        anchor.margins.bottom: dock.horizontal ? 6 : 0
        anchor.margins.left: dock.position === "left" ? 6 : 0
        anchor.margins.right: dock.position === "right" ? 6 : 0
        anchor.adjustment: PopupAdjustment.Slide | PopupAdjustment.Flip

        Rectangle {
            anchors.fill: parent
            radius: Services.Ui.radiusSm
            color: Services.Colors.background
            border.color: Services.Colors.color8
            border.width: 1

            Components.StyledText {
                id: tooltipLabel
                anchors.centerIn: parent
                text: dock.tooltipText
                font.pixelSize: 12
            }
        }
    }

    PopupWindow {
        id: contextMenu
        visible: false
        grabFocus: true
        color: "transparent"
        implicitWidth: contextButton.implicitWidth + 16
        implicitHeight: contextButton.implicitHeight + 16
        anchor.window: dock
        anchor.edges: tooltip.anchor.edges
        anchor.gravity: tooltip.anchor.gravity
        anchor.margins.bottom: tooltip.anchor.margins.bottom
        anchor.margins.left: tooltip.anchor.margins.left
        anchor.margins.right: tooltip.anchor.margins.right
        anchor.adjustment: PopupAdjustment.Slide | PopupAdjustment.Flip
        onClosed: visible = false
        onVisibleChanged: if (!visible) dock.scheduleHide()

        FocusScope {
            anchors.fill: parent
            focus: true
            Keys.onEscapePressed: contextMenu.visible = false
            Keys.onReturnPressed: contextButton.keyActivate()
            Keys.onEnterPressed: contextButton.keyActivate()

            Rectangle {
                anchors.fill: parent
                radius: Services.Ui.radiusMd
                color: Services.Colors.background
                border.color: Services.Colors.accent
                border.width: 1

                Components.TextButton {
                    id: contextButton
                    anchors.centerIn: parent
                    text: dock.contextApp?.pinned ? "Unpin from Dock" : "Pin to Dock"
                    onClicked: {
                        if (!dock.contextApp)
                            return;
                        Services.Settings.toggleDockApp(dock.contextApp.entryId);
                        contextMenu.visible = false;
                    }
                }
            }
        }
    }
}
