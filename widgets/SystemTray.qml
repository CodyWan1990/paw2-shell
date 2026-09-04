import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Services.SystemTray
import "../services" as Services
import "../components" as Components

/// System tray — renders StatusNotifierItem icons; left-click activates, right-click opens the menu.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    Layout.fillHeight: true
    implicitWidth: trayRow.implicitWidth
    visible: !(instance.hideWhenEmpty ?? true) || (SystemTray.items.values?.length ?? 0) > 0

    readonly property int iconSize: instance.iconSize ?? 20

    Components.BarItem {
        style: root.instance.style ?? "underline"
        accentColor: Services.Colors.byKey(root.instance.color, Services.Colors.trayUnderline)

    RowLayout {
        id: trayRow
        anchors.fill: parent
        spacing: root.instance.itemSpacing ?? 4

        Repeater {
            model: SystemTray.items

            delegate: Item {
                required property var modelData
                Layout.fillHeight: true
                Layout.preferredWidth: root.iconSize + 8
                Layout.topMargin: 1
                Layout.bottomMargin: 1

                Image {
                    id: trayIcon
                    anchors.centerIn: parent
                    width: root.iconSize
                    height: root.iconSize
                    source: modelData.icon
                    sourceSize.width: root.iconSize
                    sourceSize.height: root.iconSize
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    onClicked: event => {
                        if (event.button === Qt.RightButton) {
                            if (modelData.hasMenu) {
                                let pos = mapToItem(null, event.x, event.y);
                                modelData.display(root.bar, pos.x, pos.y);
                            }
                        } else if (event.button === Qt.MiddleButton) {
                            modelData.secondaryActivate();
                        } else {
                            if (!modelData.onlyMenu) {
                                modelData.activate();
                            } else if (modelData.hasMenu) {
                                let pos = mapToItem(null, event.x, event.y);
                                modelData.display(root.bar, pos.x, pos.y);
                            }
                        }
                    }
                    onWheel: wheel => {
                        modelData.scroll(wheel.angleDelta.y, false);
                    }
                }
            }
        }
    }

    }
}
