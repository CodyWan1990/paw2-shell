import QtQuick
import QtQuick.Layouts
import "../../services" as Services
import "../../components" as Components

/// Bar settings — appearance + widget layout editor over Settings.barLeft/barCenter/barRight.
Flickable {
    id: root
    anchors.fill: parent
    contentHeight: col.implicitHeight + 16
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    interactive: draggingId === ""   // don't flick while dragging a row

    // ─── keyboard row cursor, over the appearance card only ───
    // The widget layout below it is a drag-and-drop editor with its own
    // notion of "selected"; scoping the cursor to the card keeps the two from
    // fighting rather than half-navigating a list you cannot reorder anyway.
    readonly property int cursorIndex: nav.cursorIndex
    function stepCursor(delta) { return nav.stepCursor(delta); }
    function activate() { nav.activate(); }
    function adjust(delta) { nav.adjust(delta); }
    function clearCursor() { nav.clearCursor(); }

    property Components.KeyNavCursor nav: Components.KeyNavCursor {
        scope: appearanceCard
        flick: root
    }

    Component.onCompleted: nav.refresh()
    Connections {
        target: appearanceCard
        function onImplicitHeightChanged() { root.nav.refresh(); }
    }

    // Mirrors TopBar.qml widgetRegistry + inline widget keys.
    readonly property var availableWidgets: [
        "workspaces", "windowTitle", "clock", "updates", "weather", "media",
        "cpu", "memory", "sysMonitor", "audio", "systemTray", "diskUsage",
        "network", "uptime", "brightness", "battery", "themeSwitcher", "layout", "notifications",
        "menu", "power", "emoji", "separator", "spacer", "custom"
    ]

    readonly property var sectionOrder: ["left", "center", "right"]

    function arrayFor(sec) {
        if (sec === "left") return Services.Settings.barLeft;
        if (sec === "center") return Services.Settings.barCenter;
        return Services.Settings.barRight;
    }
    function setArray(sec, arr) {
        if (sec === "left") Services.Settings.barLeft = arr;
        else if (sec === "center") Services.Settings.barCenter = arr;
        else Services.Settings.barRight = arr;
    }
    function removeAt(sec, i) {
        let a = arrayFor(sec).slice(); a.splice(i, 1); setArray(sec, a);
    }
    function moveToSection(fromSec, index, toSec) {
        if (fromSec === toSec) return;
        let from = arrayFor(fromSec).slice();
        let item = from.splice(index, 1)[0];
        setArray(fromSec, from);
        let to = arrayFor(toSec).slice();
        to.push(item);
        setArray(toSec, to);
    }
    function add(sec, type, openEditor) {
        let inst = Services.Settings.normalizeWidget({ type: type });
        if (type === "custom") inst.intervalMs = 5000;
        let a = arrayFor(sec).slice(); a.push(inst); setArray(sec, a);
        if (openEditor)
            editingId = inst.id;
    }

    function ensureVisible(item) {
        if (!item || height <= 0)
            return;
        const top = item.mapToItem(contentItem, 0, 0).y;
        const itemHeight = item.height || item.implicitHeight || 0;
        const bottom = top + itemHeight;
        const pad = 16;
        const maximum = Math.max(0, contentHeight - height);
        if (itemHeight + pad * 2 > height || top - pad < contentY)
            contentY = Math.max(0, Math.min(maximum, top - pad));
        else if (bottom + pad > contentY + height)
            contentY = Math.min(maximum, bottom + pad - height);
    }

    function moveWidget(id, toSec, toIndex) {
        let loc = locate(id);
        if (!loc) return;
        /* A click on the drag handle is a full press/drop cycle that lands on the
         * row's own DropArea. Without this the no-op costs a section rebuild and
         * a disk write. */
        if (loc.sec === toSec && (toIndex === loc.index || toIndex === loc.index + 1))
            return;
        let widget = loc.widget;
        if (loc.sec === toSec) {
            let a = arrayFor(toSec).slice();
            a.splice(loc.index, 1);
            let idx = toIndex;
            if (idx > loc.index) idx--;             // account for the removed slot
            idx = Math.max(0, Math.min(idx, a.length));
            a.splice(idx, 0, widget);
            setArray(toSec, a);
        } else {
            let from = arrayFor(loc.sec).slice();
            from.splice(loc.index, 1);
            setArray(loc.sec, from);
            let to = arrayFor(toSec).slice();
            let idx = Math.max(0, Math.min(toIndex, to.length));
            to.splice(idx, 0, widget);
            setArray(toSec, to);
        }
    }

    property string draggingId: ""

    property string editingId: ""   // id of the widget whose style editor is open

    function locate(id) {
        for (let s of sectionOrder) {
            let arr = arrayFor(s);
            for (let i = 0; i < arr.length; i++)
                if (arr[i].id === id) return { sec: s, index: i, widget: arr[i] };
        }
        return null;
    }
    function updateWidget(id, patch) {
        let loc = locate(id);
        if (!loc) return;
        let a = arrayFor(loc.sec).slice();
        a[loc.index] = Object.assign({}, a[loc.index], patch);
        setArray(loc.sec, a);
    }

    Column {
        id: col
        width: root.width
        topPadding: 16
        bottomPadding: 16
        spacing: 14

        Components.Card {
            id: appearanceCard
            x: 16
            width: col.width - 32
            title: "Bar Appearance"

            Components.StyledText {
                width: parent.width
                text: "Position and size of the bar. Gap is the margin a floating bar keeps from the screen edge — window gaps are the compositor's, on the Compositor tab. Radius rounds the bar and the rest of the shell UI. Hide vacant tags sets the default for workspaces widgets, which can override it individually; selected and urgent tags remain visible."
                color: Services.Colors.color8
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Components.SegmentedControl {
                label: "Position"
                model: ["top", "bottom"]
                currentValue: Services.Settings.barPosition
                onSelected: value => Services.Settings.barPosition = value
            }

            Components.StyledSlider {
                label: "Height"; from: 24; to: 60; stepSize: 1
                value: Services.Settings.barHeight
                onMoved: v => Services.Settings.barHeight = Math.round(v)
            }

            Components.StyledSlider {
                label: "Opacity"; from: 0; to: 1; stepSize: 0.05; decimals: 2
                value: Services.Settings.barOpacity
                onMoved: v => Services.Settings.barOpacity = v
            }

            Components.SegmentedControl {
                label: "Floating"
                model: [{ value: "true", label: "On" }, { value: "false", label: "Off" }]
                valueRole: "value"; textRole: "label"
                currentValue: Services.Settings.barFloating ? "true" : "false"
                onSelected: value => Services.Settings.barFloating = (value === "true")
            }

            Components.SegmentedControl {
                label: "Hide vacant tags by default"
                model: [{ value: "true", label: "On" }, { value: "false", label: "Off" }]
                valueRole: "value"; textRole: "label"
                currentValue: Services.Settings.barHideVacantTags ? "true" : "false"
                onSelected: value => Services.Settings.barHideVacantTags = (value === "true")
            }

            Components.StyledSlider {
                label: "Gap"; from: 0; to: 40; stepSize: 1; suffix: "px"
                value: Services.Settings.barGap
                onMoved: v => Services.Settings.barGap = Math.round(v)
            }

            Components.StyledSlider {
                label: "Radius"; from: 0; to: 30; stepSize: 1; suffix: "px"
                value: Services.Settings.barRadius
                onMoved: v => Services.Settings.barRadius = Math.round(v)
            }

            Components.SegmentedControl {
                label: "Shadow"
                model: [{ value: "true", label: "On" }, { value: "false", label: "Off" }]
                valueRole: "value"; textRole: "label"
                currentValue: Services.Settings.barShadowEnabled ? "true" : "false"
                onSelected: value => Services.Settings.barShadowEnabled = value === "true"
            }

            Components.ShadowSettings {
                width: parent.width
                target: "bar"
                enabled: Services.Settings.barShadowEnabled
            }
        }

        // Widget layout — one card per section
        Repeater {
            model: root.sectionOrder

            delegate: Components.Card {
                id: secCard
                required property string modelData
                required property int index
                x: 16
                width: col.width - 32
                title: modelData.charAt(0).toUpperCase() + modelData.slice(1)

                readonly property var items: root.arrayFor(modelData)

                Components.StyledText {
                    visible: secCard.items.length === 0
                    width: parent.width
                    text: "No widgets here yet — add one below."
                    color: Services.Colors.color8
                    font.pixelSize: 13
                    font.italic: true
                }

                Repeater {
                    model: secCard.items

                    delegate: Column {
                        id: rowWrap
                        required property var modelData
                        required property int index
                        width: parent.width
                        spacing: 0

                        Rectangle {
                            id: rowItem
                            readonly property var modelData: rowWrap.modelData
                            readonly property int index: rowWrap.index
                            readonly property bool editing: root.editingId === rowWrap.modelData.id
                            width: parent.width
                            height: 44
                            radius: 8
                            opacity: root.draggingId === rowWrap.modelData.id ? 0.4 : 1.0
                            color: editing
                                ? Services.Colors.accentAlpha(0.18)
                                : (rowHover.hovered ? Services.Colors.accentAlpha(0.12) : "transparent")

                            HoverHandler { id: rowHover }

                            // Drop target: insert the dragged widget before this row.
                            DropArea {
                                anchors.fill: parent
                                keys: ["barwidget"]
                                /* Deferred: reordering reassigns the section array,
                                 * which rebuilds every row delegate — including the
                                 * MouseArea whose onReleased is running us right now.
                                 * Let it finish before pulling the rug. */
                                onDropped: {
                                    let id = root.draggingId, sec = secCard.modelData, idx = rowItem.index;
                                    Qt.callLater(() => root.moveWidget(id, sec, idx));
                                }

                                Rectangle {
                                    anchors { left: parent.left; right: parent.right; top: parent.top }
                                    height: 2
                                    color: Services.Colors.accent
                                    visible: parent.containsDrag
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    text: "☰"
                                    color: Services.Colors.color8
                                    font.pixelSize: 16
                                    Layout.alignment: Qt.AlignVCenter

                                    MouseArea {
                                        id: rowDrag
                                        anchors.fill: parent
                                        anchors.margins: -6
                                        cursorShape: root.draggingId === rowItem.modelData.id
                                            ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        drag.target: dragGhost
                                        onPressed: mouse => {
                                            dragGhost.label = rowItem.modelData.type;
                                            let gp = mapToItem(root.contentItem, mouse.x, mouse.y);
                                            dragGhost.x = gp.x - dragGhost.width / 2;
                                            dragGhost.y = gp.y - dragGhost.height / 2;
                                            root.draggingId = rowItem.modelData.id;
                                            dragGhost.Drag.active = true;
                                        }
                                        onReleased: {
                                            dragGhost.Drag.drop();
                                            dragGhost.Drag.active = false;
                                            root.draggingId = "";
                                        }
                                        /* No released delivery when the panel closes
                                         * mid-drag, and draggingId gates interactive
                                         * and the ghost — strand it and the tab is
                                         * dead until the Loader recreates it. */
                                        onCanceled: {
                                            dragGhost.Drag.cancel();
                                            dragGhost.Drag.active = false;
                                            root.draggingId = "";
                                        }
                                    }
                                }

                                Components.StyledText {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: rowItem.modelData.type
                                        + (rowItem.modelData.type === "custom" && rowItem.modelData.label
                                            ? " · " + rowItem.modelData.label : "")
                                    color: Services.Colors.foreground
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }

                                // Section pill — L | C | R
                                Row {
                                    spacing: 0
                                    Layout.alignment: Qt.AlignVCenter

                                    Repeater {
                                        model: root.sectionOrder
                                        delegate: Rectangle {
                                            required property string modelData
                                            width: 26
                                            height: 24
                                            property bool cur: secCard.modelData === modelData
                                            color: cur ? Services.Colors.accent : "transparent"
                                            border.color: Services.Colors.color8
                                            border.width: 1

                                            Components.StyledText {
                                                anchors.centerIn: parent
                                                text: modelData.charAt(0).toUpperCase()
                                                color: parent.cur ? Services.Colors.background : Services.Colors.color8
                                                font.pixelSize: 12
                                                font.bold: parent.cur
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.moveToSection(secCard.modelData, rowItem.index, modelData)
                                            }
                                        }
                                    }
                                }

                                Components.ChipButton {
                                    symbol: "⚙"
                                    size: 16
                                    active: rowItem.editing
                                    Layout.alignment: Qt.AlignVCenter
                                    onClicked: root.editingId =
                                        (root.editingId === rowItem.modelData.id ? "" : rowItem.modelData.id)
                                }

                                Components.ChipButton {
                                    symbol: "✕"
                                    size: 16
                                    activeColor: Services.Colors.red
                                    Layout.alignment: Qt.AlignVCenter
                                    onClicked: root.removeAt(secCard.modelData, rowItem.index)
                                }
                            }

                            Rectangle {
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                                height: 1
                                color: Services.Colors.color8
                                opacity: 0.2
                                visible: !rowItem.editing && rowItem.index < secCard.items.length - 1
                            }
                        }

                        // Inline style editor — Loader adopts loaded panel's implicitHeight (0 when inactive).
                        Loader {
                            width: rowWrap.width
                            active: root.editingId === rowWrap.modelData.id
                            visible: active
                            sourceComponent: styleEditorComponent
                            onLoaded: Qt.callLater(() => root.ensureVisible(rowWrap))
                        }
                    }
                }

                // Drop-at-end target
                Item {
                    width: parent.width
                    height: 8

                    DropArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        keys: ["barwidget"]
                        onDropped: {
                            let id = root.draggingId, sec = secCard.modelData;
                            let idx = root.arrayFor(sec).length;
                            Qt.callLater(() => root.moveWidget(id, sec, idx));
                        }

                        Rectangle {
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                            height: 2
                            color: Services.Colors.accent
                            visible: parent.containsDrag
                        }
                    }
                }

                Components.TextButton {
                    width: parent.width
                    text: "+ Add separator"
                    onClicked: root.add(secCard.modelData, "separator", false)
                }

                Components.StyledDropdown {
                    id: addWidgetDropdown
                    width: parent.width
                    label: "+ Add widget"
                    model: root.availableWidgets
                    currentValue: ""
                    onSelected: value => root.add(secCard.modelData, value, true)
                    onImplicitHeightChanged: if (expanded) root.ensureVisible(addWidgetDropdown)
                }
            }
        }
    }

    // Inline per-widget style editor (loaded under the row being edited)
    Component {
        id: styleEditorComponent

        Rectangle {
            id: ed
            width: parent ? parent.width : 0
            implicitHeight: edCol.implicitHeight + 20
            radius: 8
            color: Services.Colors.accentAlpha(0.07)

            readonly property var loc: root.editingId !== "" ? root.locate(root.editingId) : null
            readonly property var specs: loc ? (Services.Settings.widgetSpecs[loc.widget.type] ?? []) : []

            /// Current value of a spec's key on the widget being edited.
            function optValue(spec) {
                if (!loc) return spec.fallback;
                let v = loc.widget[spec.key];
                return v !== undefined ? v : spec.fallback;
            }
            function setOpt(spec, value) {
                let patch = {};
                patch[spec.key] = value;
                root.updateWidget(root.editingId, patch);
            }

            Rectangle {
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: 3
                color: Services.Colors.accent
            }

            Column {
                id: edCol
                x: 16
                y: 10
                width: ed.width - 28
                spacing: 8

                Components.SegmentedControl {
                    label: "Shape"
                    model: ["none", "underline", "box", "pill"]
                    currentValue: ed.loc ? ed.loc.widget.style : "underline"
                    onSelected: value => root.updateWidget(root.editingId, { style: value })
                }

                Components.StyledDropdown {
                    id: colorDropdown
                    width: parent.width
                    label: "Color"
                    model: Services.Colors.paletteKeys
                    currentValue: ed.loc ? ed.loc.widget.color : "auto"
                    onSelected: value => root.updateWidget(root.editingId, { color: value })
                    onImplicitHeightChanged: if (expanded) root.ensureVisible(colorDropdown)
                }

                Components.SegmentedControl {
                    label: "Icon"
                    model: [{ value: "none", label: "None" }, { value: "emoji", label: "Emoji" }, { value: "nerd", label: "Nerd" }]
                    valueRole: "value"; textRole: "label"
                    currentValue: ed.loc ? (ed.loc.widget.iconSet || "emoji") : "emoji"
                    onSelected: value => root.updateWidget(root.editingId, { iconSet: value })
                }

                // Type-specific options, straight off Settings.widgetSpecs. The
                // header only appears for types that actually declare options.
                Components.SectionHeader {
                    width: parent.width
                    text: "Options"
                    showDivider: false
                    visible: ed.specs.length > 0
                }

                Repeater {
                    model: ed.specs

                    delegate: Components.SettingsOptionEditor {
                        required property var modelData
                        width: edCol.width
                        spec: modelData
                        value: ed.optValue(modelData)
                        choices: modelData.type === "palette"
                            ? Services.Colors.paletteKeys : (modelData.choices ?? [])
                        labelWidth: 110
                        onValueEdited: value => ed.setOpt(modelData, value)
                    }
                }

                Item {
                    width: parent.width; height: 30
                    Components.TextButton {
                        text: "Done"
                        horizontalPadding: 12
                        buttonHeight: 28
                        onClicked: root.editingId = ""
                    }
                }
            }
        }
    }

    // Floating drag ghost (follows the cursor while a row is dragged).
    Item {
        id: dragGhost
        width: 110; height: 28
        z: 1000
        visible: Drag.active
        property string label: ""
        Drag.active: false
        Drag.keys: ["barwidget"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: Services.Colors.accent
            opacity: 0.92
            Components.StyledText {
                anchors.centerIn: parent
                text: dragGhost.label
                color: Services.Colors.background
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                width: parent.width - 8
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
