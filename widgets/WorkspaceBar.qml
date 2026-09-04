import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// Workspace indicators for one monitor; click to switch.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    Layout.fillHeight: true
    /// Hidden without paw2, which is the only source of workspace state.
    visible: Services.Paw.connected
    implicitWidth: visible ? row.implicitWidth : 0

    // workspace.output matches the connector name.
    readonly property string outputName: bar && bar.screen ? bar.screen.name : ""

    readonly property var allWsList: Services.Paw.workspaces
        .filter(w => w.output === root.outputName)
        .slice().sort((a, b) => a.idx - b.idx)

    readonly property var wsList: {
        let list = root.allWsList;
        const vacantMode = root.instance.vacantMode ?? "inherit";
        const hideVacant = vacantMode === "hide"
            || (vacantMode === "inherit" && Services.Settings.barHideVacantTags);
        if (hideVacant) {
            // Selected stays visible so the widget never empties out and leaves
            // nothing to click; urgent stays so an alert is never swallowed.
            list = list.filter(w => w.isOccupied || w.isActive || w.isUrgent);
        }
        return list;
    }

    function cycleWorkspace(step) {
        const list = root.allWsList;
        if (list.length === 0) return;

        const activeIndex = list.findIndex(w => w.isActive);
        const targetIndex = activeIndex < 0
            ? (step > 0 ? 0 : list.length - 1)
            : (activeIndex + step + list.length) % list.length;
        Services.Paw.focusWorkspace(root.outputName, list[targetIndex].idx);
    }

    Components.BarItem {
        style: root.instance.style ?? "none"
        accentColor: Services.Colors.byKey(root.instance.color, Services.Colors.workspaceActive)

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: root.instance.itemSpacing ?? 4
        anchors.leftMargin: 2
        anchors.rightMargin: 2

        Repeater {
            model: root.wsList

            delegate: Item {
                id: wsButton
                required property var modelData

                readonly property bool focused: modelData.isFocused
                readonly property bool active: modelData.isActive && !modelData.isFocused
                readonly property bool occupied: modelData.isOccupied === true
                readonly property bool urgent: modelData.isUrgent === true
                readonly property string dispVal: (modelData.name && modelData.name.length > 0)
                    ? modelData.name : String(modelData.idx)
                readonly property color accent: Services.Colors.byKey(root.instance.color, Services.Colors.clockUnderline)
                readonly property color activeColor: Services.Colors.byKey(
                    root.instance.activeColor, Services.Colors.workspaceActive)
                readonly property color urgentColor: Services.Colors.byKey(
                    root.instance.urgentColor, Services.Colors.red)
                readonly property color hoverColor: Services.Colors.byKey(
                    root.instance.hoverColor, Services.Colors.red)
                readonly property color occupiedColor: Services.Colors.byKey(
                    root.instance.occupiedColor, Services.Colors.color7)
                readonly property color vacantColor: Services.Colors.byKey(
                    root.instance.vacantColor, Services.Colors.color8)

                Layout.fillHeight: true
                Layout.preferredWidth: wsLabel.implicitWidth
                    + 2 * (root.instance.tagPadding ?? 4)

                Components.StyledText {
                    id: wsLabel
                    anchors.centerIn: parent
                    text: Services.Fmt.fill(root.instance.format ?? "{indicator} {name}", {
                        index: wsButton.modelData.idx,
                        name: wsButton.dispVal,
                        indicator: root.instance.indicator ?? ""
                    })
                    color: wsButton.focused ? wsButton.accent
                        : wsButton.urgent ? wsButton.urgentColor
                        : wsButton.active ? wsButton.activeColor
                        : wsHover.containsMouse ? wsButton.hoverColor
                        : wsButton.occupied ? wsButton.occupiedColor
                        : wsButton.vacantColor
                    font.pixelSize: Math.round((root.bar ? root.bar.fontSize : 17)
                        * (root.instance.tagScale ?? 1.3))
                    font.bold: root.instance.bold ?? true
                    horizontalAlignment: Text.AlignHCenter
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: root.instance.underlineHeight ?? 2
                    color: wsButton.accent
                    visible: wsButton.focused && height > 0
                }

                MouseArea {
                    id: wsHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Services.Paw.focusWorkspace(wsButton.modelData.output,
                                                          wsButton.modelData.idx)
                }
            }
        }
    }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: wheel => {
                if (!(root.instance.scrollEnabled ?? false)
                        || wheel.angleDelta.y === 0) {
                    wheel.accepted = false;
                    return;
                }

                let step = wheel.angleDelta.y < 0 ? 1 : -1;
                if (root.instance.reverseScroll ?? false) step *= -1;
                root.cycleWorkspace(step);
                wheel.accepted = true;
            }
        }
    }
}
