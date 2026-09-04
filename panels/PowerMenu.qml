import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// Power menu overlay — replaces rofi-based powermenu.sh.
Components.OverlayPanel {
    id: menu

    onOpened: {
        pendingAction = "";
        pendingLabel = "";
        selectedButton = 0;
    }
    onFocusRequested: focusScope.forceActiveFocus()

    property string fontFamily: Services.Settings.fontFamily

    property string pendingAction: ""   // "logout", "reboot", "shutdown", or ""
    property string pendingLabel: ""
    property int selectedButton: 0      // 0=Logout, 1=Reboot, 2=Shutdown (or 0=No, 1=Yes in confirm)

    function requestAction(action, label) {
        pendingAction = action;
        pendingLabel = label;
    }

    function confirm() {
        powerActions.perform(pendingAction);
        if (pendingAction !== "logout") close();
    }

    // Keys must be on an Item, not the PanelWindow, so the menu lives inside a
    // FocusScope rather than beside one — as a sibling it only ever saw a key
    // while it personally held focus, which is why none of these fired.
    FocusScope {
        id: focusScope
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: {
            if (menu.pendingAction !== "")
                menu.pendingAction = "";
            else
                menu.close();
        }
        Keys.onLeftPressed: {
            if (menu.pendingAction !== "")
                menu.selectedButton = Math.max(0, menu.selectedButton - 1);
            else
                menu.selectedButton = Math.max(0, menu.selectedButton - 1);
        }
        Keys.onRightPressed: {
            let max = menu.pendingAction !== "" ? 1 : 2;
            menu.selectedButton = Math.min(max, menu.selectedButton + 1);
        }
        Keys.onReturnPressed: {
            if (menu.pendingAction !== "") {
                if (menu.selectedButton === 0) menu.pendingAction = "";
                else menu.confirm();
            } else {
                let actions = ["logout", "reboot", "shutdown"];
                let labels = ["Logout", "Reboot", "Shutdown"];
                menu.requestAction(actions[menu.selectedButton], labels[menu.selectedButton]);
                menu.selectedButton = 0;
            }
        }

        Components.OverlaySurface {
            anchors.centerIn: parent
            width: 380
            height: menu.pendingAction !== "" ? 220 : 180
            opacity: menu.contentOpacity

            Behavior on height {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Text {
                    text: "\uF011  Power Menu"
                    color: Services.Colors.foreground
                    font.family: menu.fontFamily
                    font.pixelSize: 19
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Rectangle { width: parent.width; height: 1; color: Services.Colors.color8 }

                // ─── Action buttons (hidden during confirmation) ───
                Components.PowerActions {
                    id: powerActions
                    width: parent.width
                    spacing: 10
                    visible: menu.pendingAction === ""
                    buttonHeight: 50
                    showIcons: true
                    confirm: true
                    selectedIndex: menu.pendingAction === "" ? menu.selectedButton : -1
                    onRequested: (action, label) => menu.requestAction(action, label)
                    onHovered: index => menu.selectedButton = index
                }

                // ─── Confirmation (shown after selecting an action) ───
                Column {
                    width: parent.width
                    spacing: 12
                    visible: menu.pendingAction !== ""

                    Text {
                        textFormat: Text.PlainText
                        text: menu.pendingLabel + "?"
                        color: Services.Colors.foreground
                        font.family: menu.fontFamily
                        font.pixelSize: 17
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    RowLayout {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 16

                        Rectangle {
                            property bool active: cancelHover.containsMouse || (menu.pendingAction !== "" && menu.selectedButton === 0)
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 40
                            color: active ? Services.Colors.color8 : Services.Colors.color0
                            border.color: active ? Services.Colors.foreground : Services.Colors.color8
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "No"
                                color: parent.active ? Services.Colors.foreground : Services.Colors.color8
                                font.family: menu.fontFamily
                                font.pixelSize: 15
                            }
                            MouseArea {
                                id: cancelHover
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: menu.pendingAction = ""
                                onContainsMouseChanged: if (containsMouse) menu.selectedButton = 0
                            }
                        }

                        Rectangle {
                            property bool active: confirmHover.containsMouse || (menu.pendingAction !== "" && menu.selectedButton === 1)
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 40
                            color: active ? Services.Colors.red : Services.Colors.color0
                            border.color: active ? Services.Colors.red : Services.Colors.color8
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Yes"
                                color: parent.active ? Services.Colors.foreground : Services.Colors.red
                                font.family: menu.fontFamily
                                font.pixelSize: 15
                            }
                            MouseArea {
                                id: confirmHover
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: menu.confirm()
                                onContainsMouseChanged: if (containsMouse) menu.selectedButton = 1
                            }
                        }
                    }
                }

                Text {
                    text: "Press Escape to cancel"
                    color: Services.Colors.color8
                    font.family: menu.fontFamily
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
