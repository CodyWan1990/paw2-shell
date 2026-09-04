import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// HSV color picker (S/V square + hue slider + hex field); hex field is updated imperatively (never bound) so dragging stays live.
Item {
    id: root
    width: parent ? parent.width : 260
    implicitHeight: pickerCol.implicitHeight

    property real h: 0
    property real s: 0
    property real v: 0
    readonly property color value: Qt.hsva(h, s, v, 1)

    signal edited(color value)

    property bool _loading: false

    function _hexToColor(t) {
        t = t.replace("#", "");
        return Qt.rgba(parseInt(t.substr(0, 2), 16) / 255,
                       parseInt(t.substr(2, 2), 16) / 255,
                       parseInt(t.substr(4, 2), 16) / 255, 1);
    }

    function toHex(c) {
        function p(x) { return Math.round(x * 255).toString(16).padStart(2, '0'); }
        return "#" + p(c.r) + p(c.g) + p(c.b);
    }

    /// Set h/s/v from a color (or "#rrggbb" string) without emitting.
    function load(c) {
        if (typeof c === "string") c = _hexToColor(c);
        _loading = true;
        if (c.hsvHue >= 0) h = c.hsvHue;   // hsvHue is -1 for achromatic; keep current
        s = c.hsvSaturation;
        v = c.hsvValue;
        _loading = false;
        hexField.text = toHex(value);
    }

    function _changed() {
        hexField.text = toHex(value);
        if (!_loading) root.edited(root.value);
    }

    Column {
        id: pickerCol
        width: parent.width
        spacing: 10

        Row {
            width: parent.width
            height: 150
            spacing: 10

            Item {
                id: sv
                width: parent.width - 28
                height: parent.height

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: Qt.hsva(root.h, 1, 1, 1)

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: "#ffffffff" }
                            GradientStop { position: 1; color: "#00ffffff" }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0; color: "#00000000" }
                            GradientStop { position: 1; color: "#ff000000" }
                        }
                    }
                }

                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: "transparent"
                    border.color: "#ffffff"; border.width: 2
                    x: root.s * sv.width - 6
                    y: (1 - root.v) * sv.height - 6
                }

                MouseArea {
                    anchors.fill: parent
                    // Hosted in a Flickable (ThemeBuilder tab) — don't let it steal drags.
                    preventStealing: true
                    function set(mx, my) {
                        root.s = Math.max(0, Math.min(1, mx / sv.width));
                        root.v = Math.max(0, Math.min(1, 1 - my / sv.height));
                        root._changed();
                    }
                    onPressed: e => set(e.x, e.y)
                    onPositionChanged: e => { if (pressed) set(e.x, e.y); }
                }
            }

            Item {
                id: hue
                width: 18
                height: parent.height

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.000; color: "#ff0000" }
                        GradientStop { position: 0.167; color: "#ffff00" }
                        GradientStop { position: 0.333; color: "#00ff00" }
                        GradientStop { position: 0.500; color: "#00ffff" }
                        GradientStop { position: 0.667; color: "#0000ff" }
                        GradientStop { position: 0.833; color: "#ff00ff" }
                        GradientStop { position: 1.000; color: "#ff0000" }
                    }
                }

                Rectangle {
                    width: parent.width + 4; height: 4; radius: 2
                    x: -2
                    y: root.h * hue.height - 2
                    color: "transparent"
                    border.color: "#ffffff"; border.width: 2
                }

                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    function set(my) {
                        root.h = Math.max(0, Math.min(0.9999, my / hue.height));
                        root._changed();
                    }
                    onPressed: e => set(e.y)
                    onPositionChanged: e => { if (pressed) set(e.y); }
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 10

            SettingField {
                id: hexField
                Layout.fillWidth: true
                Component.onCompleted: text = root.toHex(root.value)
                onEdited: txt => {
                    let t = txt.trim();
                    if (!t.startsWith("#")) t = "#" + t;
                    if (/^#[0-9a-fA-F]{6}$/.test(t)) {
                        root.load(root._hexToColor(t));
                        root.edited(root.value);
                    } else {
                        hexField.text = root.toHex(root.value);   // revert invalid input
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 32
                radius: 4
                color: root.value
                border.color: Services.Colors.color8
                border.width: 1
            }
        }
    }
}
