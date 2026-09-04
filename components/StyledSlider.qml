import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Reusable labeled slider with track, fill, and thumb.
Item {
    id: root
    width: parent ? parent.width : 300
    height: 48

    property string label: ""
    property string suffix: ""
    property real from: 0
    property real to: 100
    property real value: 0
    property real stepSize: 1
    property int decimals: 0
    /// Width of the label column; widen it for long setting names.
    property int labelWidth: 100
    /// Set by a host whose keyboard cursor is on this row.
    property bool cursor: false

    signal moved(real value)

    /// Keyboard surface (see SettingsPage). A slider has nothing to open, so
    /// Enter does nothing and the arrows do the work.
    function keyActivate() {}

    function keyAdjust(delta) {
        const step = stepSize > 0 ? stepSize : (to - from) / 100;
        moved(Math.max(from, Math.min(to, value + delta * step)));
    }

    property real normalizedValue: (value - from) / (to - from)
    property string displayValue: decimals > 0
        ? value.toFixed(decimals) + suffix
        : Math.round(value) + suffix

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 12

        StyledText {
            text: root.label
            color: root.cursor ? Services.Colors.accent : Services.Colors.foreground
            font.pixelSize: 14
            font.bold: root.cursor
            Layout.preferredWidth: root.labelWidth
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: track
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 10
                radius: 5
                color: Services.Colors.color8

                Rectangle {
                    width: parent.width * root.normalizedValue
                    height: parent.height
                    radius: 5
                    color: Services.Colors.accent
                }

                // Also shown for the keyboard cursor, so the row you are about
                // to change with Left/Right is the one showing a grip.
                Rectangle {
                    x: parent.width * root.normalizedValue - 7
                    y: -2
                    width: 14
                    height: 14
                    radius: 7
                    color: sliderArea.containsMouse || sliderArea.pressed
                        ? Services.Colors.foreground
                        : Services.Colors.accent
                    visible: sliderArea.containsMouse || sliderArea.pressed || root.cursor
                }
            }

            MouseArea {
                id: sliderArea
                anchors.fill: parent
                anchors.topMargin: -8
                anchors.bottomMargin: -8
                hoverEnabled: true
                // The settings tabs are Flickables; without this the flick
                // gesture steals the grab ~10px into a drag and the slider dies.
                preventStealing: true

                function setFromMouse(mouseX) {
                    let norm = Math.max(0, Math.min(1, mouseX / track.width));
                    let raw = root.from + norm * (root.to - root.from);
                    if (root.stepSize > 0) {
                        raw = Math.round(raw / root.stepSize) * root.stepSize;
                    }
                    raw = Math.max(root.from, Math.min(root.to, raw));
                    // Don't assign root.value here — that would break the caller's
                    // `value:` binding; moved() flows back through it instead.
                    root.moved(raw);
                }

                onClicked: event => setFromMouse(event.x)
                onPositionChanged: event => { if (pressed) setFromMouse(event.x) }
            }
        }

        StyledText {
            text: root.displayValue
            color: Services.Colors.foreground
            font.pixelSize: 14
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignRight
        }
    }
}
