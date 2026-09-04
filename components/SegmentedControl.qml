import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Segmented toggle button row. String model auto-capitalizes; object model needs valueRole + textRole.
Item {
    id: control
    width: parent?.width ?? 0
    height: 40
    // Item.enabled already stops the delegates' MouseAreas, so setting it is
    // the whole disabled state; this just makes it visible.
    opacity: enabled ? 1.0 : 0.4

    property string label: ""
    property var model: []
    property string currentValue: ""
    property string valueRole: ""
    property string textRole: ""
    /// Width of the label column; widen it for long setting names.
    property int labelWidth: 100
    /// Set by a host whose keyboard cursor is on this row.
    property bool cursor: false

    signal selected(string value)

    /// Keyboard surface (see SettingsPage). Values of a segmented control are a
    /// short ordered list, so Left/Right walks it and Enter takes the next one
    /// — which is what makes a two-value On/Off row a single keypress.
    function valueAt(i) {
        const item = model[i];
        if (control.valueRole !== "" && item !== null && typeof item === "object")
            return item[control.valueRole] ?? "";
        return String(item ?? "");
    }

    function currentIndex() {
        for (let i = 0; i < (model ?? []).length; i++)
            if (valueAt(i) === currentValue)
                return i;
        return 0;
    }

    function keyAdjust(delta) {
        const n = (model ?? []).length;
        if (!enabled || n === 0)
            return;
        selected(valueAt((currentIndex() + delta + n) % n));
    }

    function keyActivate() { keyAdjust(1); }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        StyledText {
            text: control.label
            color: control.cursor ? Services.Colors.accent : Services.Colors.foreground
            font.pixelSize: 14
            font.bold: control.cursor
            Layout.preferredWidth: control.labelWidth
        }

        Repeater {
            model: control.model
            delegate: Rectangle {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                radius: 4

                property string itemValue: {
                    if (control.valueRole !== "" && modelData !== null && typeof modelData === "object")
                        return modelData[control.valueRole] ?? "";
                    return String(modelData ?? "");
                }
                property string itemText: {
                    if (control.textRole !== "" && modelData !== null && typeof modelData === "object")
                        return modelData[control.textRole] ?? "";
                    let s = String(modelData ?? "");
                    return s.charAt(0).toUpperCase() + s.slice(1);
                }
                property bool isActive: control.currentValue === itemValue

                color: isActive ? Services.Colors.accent : Services.Colors.color0
                border.color: control.cursor ? Services.Colors.accent : Services.Colors.color8
                border.width: control.cursor ? 2 : 1

                StyledText {
                    anchors.centerIn: parent
                    text: parent.itemText
                    color: parent.isActive
                        ? Services.Colors.background
                        : Services.Colors.foreground
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: control.selected(parent.itemValue)
                }
            }
        }
    }
}
