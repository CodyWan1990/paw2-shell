import QtQuick
import QtQuick.Layouts
import "../../services" as Services
import "../../components" as Components

/// New-theme creator overlay — extracted from AppLauncher. State and the
/// open/save/setSlotColor functions live on the `launcher` coordinator; this is
/// just the modal UI bound to it.
Rectangle {
    id: createOverlay

    required property var launcher

    visible: launcher.creatingTheme
    color: "#cc000000"

    MouseArea { anchors.fill: parent; onClicked: createOverlay.launcher.creatingTheme = false }

    // Reload the picker when the dialog opens or the active slot changes
    Connections {
        target: createOverlay.launcher
        function onActiveSlotIndexChanged() {
            if (createOverlay.launcher.creatingTheme) themePicker.load(createOverlay.launcher.activeSlotColor());
        }
        function onCreatingThemeChanged() {
            if (createOverlay.launcher.creatingTheme) {
                nameField.text = createOverlay.launcher.newThemeName;
                themePicker.load(createOverlay.launcher.activeSlotColor());
            }
        }
    }

    FocusScope {
        anchors.fill: parent
        focus: createOverlay.launcher.creatingTheme
        Keys.onEscapePressed: createOverlay.launcher.creatingTheme = false

        Components.OverlaySurface {
            id: createDialog
            anchors.centerIn: parent
            width: Math.min(660, parent.width - 32)
            height: Math.min(520, parent.height - 32)
            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                Text {
                    text: "\uf1fc  New Theme"
                    color: Services.Colors.foreground
                    font.family: createOverlay.launcher.fontFamily
                    font.pixelSize: 19
                    font.bold: true
                }

                RowLayout {
                    width: parent.width
                    spacing: 12
                    Text {
                        text: "Name"
                        color: Services.Colors.foreground
                        font.family: createOverlay.launcher.fontFamily
                        font.pixelSize: 14
                        Layout.preferredWidth: 70
                    }
                    Components.SettingField {
                        id: nameField
                        Layout.fillWidth: true
                        Component.onCompleted: text = createOverlay.launcher.newThemeName
                        // live so the Save button enables while typing, not on focus-out
                        onLiveEdited: v => createOverlay.launcher.newThemeName = v
                    }
                }

                RowLayout {
                    width: parent.width
                    height: parent.height - 150
                    spacing: 16

                    Flickable {
                        Layout.preferredWidth: 240
                        Layout.fillHeight: true
                        contentHeight: slotCol.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Column {
                            id: slotCol
                            width: parent.width
                            spacing: 4

                            Repeater {
                                model: createOverlay.launcher.themeSlots
                                delegate: Components.InteractiveListRow {
                                    required property var modelData
                                    required property int index
                                    width: slotCol.width
                                    height: 36
                                    radius: 6
                                    selected: index === createOverlay.launcher.activeSlotIndex
                                    selectedColor: Services.Colors.accentAlpha(0.2)
                                    hoverColor: Services.Colors.accentAlpha(0.1)
                                    onClicked: createOverlay.launcher.activeSlotIndex = index

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 10

                                        Rectangle {
                                            Layout.preferredWidth: 22
                                            Layout.preferredHeight: 22
                                            radius: 4
                                            color: createOverlay.launcher.newPalette[modelData.key] || "#000000"
                                            border.color: Services.Colors.color8
                                            border.width: 1
                                        }
                                        Text {
                                            textFormat: Text.PlainText
                                            text: modelData.label
                                            Layout.fillWidth: true
                                            color: Services.Colors.foreground
                                            font.family: createOverlay.launcher.fontFamily
                                            font.pixelSize: 14
                                        }
                                        Text {
                                            textFormat: Text.PlainText
                                            text: createOverlay.launcher.newPalette[modelData.key] || ""
                                            color: Services.Colors.color8
                                            font.family: createOverlay.launcher.fontFamily
                                            font.pixelSize: 12
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Components.ColorPicker {
                        id: themePicker
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        onEdited: c => createOverlay.launcher.setSlotColor(
                            createOverlay.launcher.themeSlots[createOverlay.launcher.activeSlotIndex].key, themePicker.toHex(c))
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Components.TextButton {
                        text: "Cancel"
                        fontSize: 14
                        buttonHeight: 34
                        radius: 6
                        idleColor: "transparent"
                        hoverColor: Services.Colors.color0
                        hoverTextColor: Services.Colors.foreground
                        hoverBorderColor: Services.Colors.color8
                        onClicked: createOverlay.launcher.creatingTheme = false
                    }

                    Components.TextButton {
                        id: saveBtn
                        readonly property bool ready: createOverlay.launcher.newThemeName.trim() !== ""
                        text: "Save & Apply"
                        fontSize: 14
                        fontBold: true
                        buttonHeight: 34
                        radius: 6
                        enabled: ready
                        idleColor: Services.Colors.accent
                        idleBorderColor: Services.Colors.accent
                        idleTextColor: Services.Colors.background
                        onClicked: createOverlay.launcher.saveNewTheme()
                    }
                }
            }
        }
    }
}
