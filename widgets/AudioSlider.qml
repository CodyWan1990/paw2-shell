import QtQuick
import QtQuick.Layouts
import Quickshell.Services.Pipewire
import "../services" as Services
import "../components" as Components

/// Volume widget — scroll to adjust, left-click to mute, right-click opens slider popup.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.pulseaudioUnderline)
    Layout.fillHeight: true
    implicitWidth: audioItem.implicitWidth

    // Bind the default sink + source so properties stay live.
    PwObjectTracker {
        objects: [Pipewire.defaultAudioSink, Pipewire.defaultAudioSource]
    }

    property var sink: Pipewire.defaultAudioSink
    property real volume: sink?.audio?.volume ?? 0.5
    property bool muted: sink?.audio?.muted ?? false
    property int volumePercent: Math.round(volume * 100)

    property var source: Pipewire.defaultAudioSource
    property real micVolume: source?.audio?.volume ?? 0.5
    property bool micMuted: source?.audio?.muted ?? false
    property int micPercent: Math.round(micVolume * 100)

    function volumeIcon() {
        let set = root.instance.iconSet ?? "emoji";
        let acc = root.accent;
        if (muted) return Services.Icons.prefix("vol-muted", set, acc);
        if (volumePercent < 33) return Services.Icons.prefix("vol-low", set, acc);
        if (volumePercent < 66) return Services.Icons.prefix("vol-med", set, acc);
        return Services.Icons.prefix("vol-high", set, acc);
    }

    function micIcon() {
        let set = root.instance.iconSet ?? "emoji";
        return Services.Icons.prefix(micMuted ? "mic-muted" : "mic", set, root.accent);
    }

    Components.BarText {
        id: audioItem
        bar: root.bar
        small: false
        style: root.instance.style ?? "underline"
        accent: root.accent
        textColor: root.muted ? Services.Colors.color8 : Services.Colors.foreground
        text: root.volumeIcon() + (root.muted
            ? (root.instance.muteLabel ?? "MUTE")
            : ((root.instance.showPercent ?? true) ? root.volumePercent + "%" : ""))
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: event => {
            if (event.button === Qt.LeftButton) {
                if (root.sink?.audio) root.sink.audio.muted = !root.sink.audio.muted;
            } else if (event.button === Qt.RightButton) {
                volPopup.isOpen ? volPopup.close() : volPopup.open();
            }
        }
        onWheel: event => {
            if (!root.sink?.audio) return;
            let step = (root.instance.scrollStep ?? 5) / 100;
            let max = (root.instance.maxVolume ?? 100) / 100;
            let newVol = root.sink.audio.volume + (event.angleDelta.y > 0 ? step : -step);
            root.sink.audio.volume = Math.max(0, Math.min(max, newVol));
        }
    }

    Components.WidgetPopup {
        id: volPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 320
        accentColor: Services.Colors.pulseaudioUnderline
        xClamp: 0
        onOpening: Services.Audio.loadDevices()

        Column {
            width: parent.width
            spacing: 12

            RowLayout {
                width: parent.width
                Components.StyledText {
                    // RichText because Icons.prefix() emits a <font color> span in
                    // nerd mode. Safe here: the glyph markup is ours and "Output"
                    // is a literal, so there is no untrusted input to escape.
                    text: root.volumeIcon() + "Output"
                    textFormat: Text.RichText
                    color: Services.Colors.foreground
                    font.pixelSize: 15
                    font.bold: true
                    Layout.fillWidth: true
                }
                Components.StyledText {
                    text: root.muted ? "MUTE" : root.volumePercent + "%"
                    color: root.muted ? Services.Colors.color8 : Services.Colors.pulseaudioUnderline
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Components.PopupSlider {
                value: root.volume
                accentColor: Services.Colors.pulseaudioUnderline
                muted: root.muted
                onMoved: val => { if (root.sink?.audio) root.sink.audio.volume = val; }
            }

            Column {
                width: parent.width
                spacing: 2
                visible: Services.Audio.sinks.length > 1

                Repeater {
                    model: Services.Audio.sinks
                    delegate: Components.SelectableListItem {
                        required property var modelData
                        selected: modelData.name === Services.Audio.defaultSink
                        accentColor: Services.Colors.pulseaudioUnderline
                        label: modelData.description || modelData.name || "Unknown"
                        onClicked: Services.Audio.setDefaultSink(modelData.name)
                    }
                }
            }

            Components.Separator { }

            RowLayout {
                width: parent.width
                Components.StyledText {
                    text: root.micIcon() + "Input"
                    textFormat: Text.RichText
                    color: Services.Colors.foreground
                    font.pixelSize: 15
                    font.bold: true
                    Layout.fillWidth: true
                }
                Components.StyledText {
                    text: root.micMuted ? "MUTE" : root.micPercent + "%"
                    color: root.micMuted ? Services.Colors.color8 : Services.Colors.pulseaudioUnderline
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Components.PopupSlider {
                value: root.micVolume
                accentColor: Services.Colors.pulseaudioUnderline
                muted: root.micMuted
                onMoved: val => { if (root.source?.audio) root.source.audio.volume = val; }
            }

            Rectangle {
                width: parent.width
                height: 28
                radius: 4
                color: root.micMuted
                    ? Services.Colors.withAlpha(Services.Colors.color1, 0.2)
                    : (micMuteMa.containsMouse ? Services.Colors.color0 : "transparent")

                Components.StyledText {
                    anchors.centerIn: parent
                    text: root.micMuted ? "Unmute Microphone" : "Mute Microphone"
                    color: root.micMuted ? Services.Colors.color1 : Services.Colors.color8
                    font.pixelSize: 12
                }

                MouseArea {
                    id: micMuteMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.source?.audio)
                            root.source.audio.muted = !root.source.audio.muted;
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 2
                visible: Services.Audio.sources.length > 1

                Repeater {
                    model: Services.Audio.sources
                    delegate: Components.SelectableListItem {
                        required property var modelData
                        selected: modelData.name === Services.Audio.defaultSource
                        accentColor: Services.Colors.pulseaudioUnderline
                        label: modelData.description || modelData.name || "Unknown"
                        onClicked: Services.Audio.setDefaultSource(modelData.name)
                    }
                }
            }
        }
    }
}
