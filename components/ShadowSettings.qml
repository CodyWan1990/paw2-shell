import QtQuick
import "../services" as Services

/// Paw2-style shadow controls shared by the Bar and Dock settings tabs.
Item {
    id: root

    required property string target
    implicitHeight: controls.implicitHeight
    opacity: enabled ? 1 : 0.5

    function key(suffix) {
        return target + "Shadow" + suffix;
    }

    Column {
        id: controls
        width: parent.width
        spacing: 0

        StyledSlider {
            width: parent.width
            label: "Shadow radius"; from: 0; to: 64; stepSize: 1; suffix: "px"
            value: Services.Settings[root.key("Radius")]
            onMoved: value => Services.Settings[root.key("Radius")] = Math.round(value)
        }

        StyledSlider {
            width: parent.width
            label: "Shadow opacity"; from: 0; to: 1; stepSize: 0.05; decimals: 2
            value: Services.Settings[root.key("Opacity")]
            onMoved: value => Services.Settings[root.key("Opacity")] = value
        }

        LabeledSettingRow {
            width: parent.width
            label: "Shadow colour"

            SettingField {
                anchors.fill: parent
                Component.onCompleted: text = Services.Settings[root.key("Color")]
                onEdited: value => {
                    const color = value.trim();
                    if (/^#[0-9a-fA-F]{6}$/.test(color))
                        Services.Settings[root.key("Color")] = color.toLowerCase();
                    text = Services.Settings[root.key("Color")];
                }
            }
        }

        StyledSlider {
            width: parent.width
            label: "Shadow offset X"; from: -64; to: 64; stepSize: 1; suffix: "px"
            value: Services.Settings[root.key("OffsetX")]
            onMoved: value => Services.Settings[root.key("OffsetX")] = Math.round(value)
        }

        StyledSlider {
            width: parent.width
            label: "Shadow offset Y"; from: -64; to: 64; stepSize: 1; suffix: "px"
            value: Services.Settings[root.key("OffsetY")]
            onMoved: value => Services.Settings[root.key("OffsetY")] = Math.round(value)
        }
    }
}
