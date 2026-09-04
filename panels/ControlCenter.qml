import QtQuick
import QtQuick.Layouts
import Quickshell.Services.Pipewire
import "../services" as Services
import "../components" as Components

/// Right-side control center panel — DND, media, volume, power, notifications.
/// One instance per bar/screen (unlike the single-instance overlays), so it
/// pins to its bar's screen instead of following compositor focus.
Components.OverlayPanel {
    id: root

    required property var bar

    screen: bar.screen
    followCompositorFocus: false
    backdropDim: 0.25
    onFocusRequested: ccFocus.forceActiveFocus()

    property string fontFamily: bar.fontFamily

    // ─── State ───
    property bool dndEnabled: Services.NotifManager.dndEnabled
    onDndEnabledChanged: Services.NotifManager.dndEnabled = dndEnabled

    property var activePlayer: Services.Media.availablePlayer

    PwObjectTracker { objects: [Pipewire.defaultAudioSink] }
    property var sink: Pipewire.defaultAudioSink
    property real volume: sink?.audio?.volume ?? 0.5
    property bool muted: sink?.audio?.muted ?? false
    property int volumePercent: Math.round(volume * 100)

    FocusScope {
        id: ccFocus
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: root.close()
    }

    Components.OverlaySurface {
        id: ccContent
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: root.bar.contentHeight
        anchors.rightMargin: 8
        width: 400
        height: Math.min(parent.height - root.bar.contentHeight - 16, contentColumn.implicitHeight + 32)
        opacity: root.contentOpacity

        Flickable {
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: contentColumn.implicitHeight
            clip: true

            Column {
                id: contentColumn
                width: parent.width
                spacing: 12

                // ═══ Header ═══
                RowLayout {
                    width: parent.width

                    Text {
                        text: "Control Center"
                        color: Services.Colors.foreground
                        font.family: root.fontFamily
                        font.pixelSize: 19
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Text {
                        text: ""
                        color: Services.Colors.color8
                        font.pixelSize: 17
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.close();
                                Services.Actions.settings();
                            }
                        }
                    }

                    Text {
                        text: ""
                        color: Services.Colors.color8
                        font.pixelSize: 17
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.close()
                        }
                    }
                }

                Components.Separator { opacity: 1.0 }

                // ═══ DND Toggle ═══
                RowLayout {
                    width: parent.width
                    height: 40

                    Text {
                        textFormat: Text.PlainText
                        text: root.dndEnabled ? "\uf1f6  Do Not Disturb" : "\uf0f3  Notifications"
                        color: Services.Colors.foreground
                        font.family: root.fontFamily
                        font.pixelSize: 15
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 24
                        radius: 12
                        color: root.dndEnabled ? Services.Colors.accent : Services.Colors.color8

                        Rectangle {
                            width: 20
                            height: 20
                            radius: 10
                            anchors.verticalCenter: parent.verticalCenter
                            x: root.dndEnabled ? parent.width - width - 2 : 2
                            color: Services.Colors.foreground

                            Behavior on x {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.dndEnabled = !root.dndEnabled
                        }
                    }
                }

                Components.Separator { }

                // ═══ Media Player (MPRIS) ═══
                Column {
                    width: parent.width
                    spacing: 8
                    visible: root.activePlayer !== null

                    Text {
                        text: "  Now Playing"
                        color: Services.Colors.foreground
                        font.family: root.fontFamily
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Components.MediaTrackInfo {
                        width: parent.width
                        player: root.activePlayer
                    }

                    Components.MediaSeekBar {
                        width: parent.width
                        player: root.activePlayer
                        trackHeight: 4
                        trackColor: Services.Colors.color8
                        seekEnabled: false
                    }

                    Components.MediaTransportControls {
                        anchors.horizontalCenter: parent.horizontalCenter
                        player: root.activePlayer
                        spacing: 24
                        sideSize: 20
                        playSize: 24
                        accentColor: Services.Colors.accent
                        pointerCursor: false
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: root.activePlayer?.identity ?? ""
                        color: Services.Colors.color8
                        font.family: root.fontFamily
                        font.pixelSize: 11
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                Components.Separator {
                    visible: root.activePlayer !== null
                }

                // ═══ Volume ═══
                Column {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        width: parent.width

                        Text {
                            textFormat: Text.PlainText
                            text: root.muted ? "\uf026" : "\uf028"
                            font.pixelSize: 17
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (root.sink?.audio) root.sink.audio.muted = !root.sink.audio.muted;
                                }
                            }
                        }

                        Text {
                            text: "Volume"
                            color: Services.Colors.foreground
                            font.family: root.fontFamily
                            font.pixelSize: 15
                            Layout.fillWidth: true
                        }

                        Text {
                            textFormat: Text.PlainText
                            text: root.volumePercent + "%"
                            color: root.muted ? Services.Colors.color8 : Services.Colors.foreground
                            font.family: root.fontFamily
                            font.pixelSize: 15
                        }
                    }

                    Components.PopupSlider {
                        value: root.volume
                        accentColor: Services.Colors.accent
                        muted: root.muted
                        trackHeight: 12
                        trackColor: Services.Colors.color8
                        showThumbOnHoverOnly: true
                        onMoved: val => { if (root.sink?.audio) root.sink.audio.volume = val; }
                    }
                }

                Components.Separator { }

                // ═══ Power Buttons ═══
                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Power"
                        color: Services.Colors.foreground
                        font.family: root.fontFamily
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Components.PowerActions {
                        width: parent.width
                    }
                }

                Components.Separator { }

                // ═══ Notifications ═══
                Column {
                    width: parent.width
                    spacing: 6

                    RowLayout {
                        width: parent.width

                        Text {
                            text: "Notifications"
                            color: Services.Colors.foreground
                            font.family: root.fontFamily
                            font.pixelSize: 15
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Clear All"
                            color: Services.Colors.color8
                            font.family: root.fontFamily
                            font.pixelSize: 13
                            visible: (Services.NotifManager.notifications.values?.length ?? 0) > 0
                            MouseArea {
                                anchors.fill: parent
                                onClicked: Services.NotifManager.dismissAll()
                            }
                        }
                    }

                    Text {
                        text: "No notifications"
                        color: Services.Colors.color8
                        font.family: root.fontFamily
                        font.pixelSize: 13
                        visible: (Services.NotifManager.notifications.values?.length ?? 0) === 0
                    }

                    Repeater {
                        // history, not notifications: newest first, which only
                        // started mattering once the list stopped self-emptying.
                        model: Services.NotifManager.history

                        delegate: Rectangle {
                            id: notifItem
                            required property var modelData
                            width: parent.width
                            height: notifContent.implicitHeight + 16
                            color: Services.Colors.color0
                            border.color: Services.Colors.color8
                            border.width: 1

                            // Click the card (except ✕ / pills) to invoke the default action.
                            MouseArea {
                                anchors.fill: parent
                                enabled: notifContent.defaultAction !== null
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: notifContent.invokeDefault()
                            }

                            Components.NotificationContent {
                                id: notifContent
                                anchors.fill: parent
                                anchors.margins: 8
                                notif: notifItem.modelData
                                contentSpacing: 2
                                summarySize: 14
                                bodySize: 13
                                bodyMaximumLines: 3
                            }
                        }
                    }
                }
            }
        }
    }
}
