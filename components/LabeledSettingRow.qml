import QtQuick
import "../services" as Services

/// Label plus an arbitrary settings control, with consistent sizing.
Item {
    id: root

    default property alias content: controlSlot.data
    property string label: ""
    property int labelWidth: 110
    property int rowHeight: 40
    property int gap: 12

    height: rowHeight

    /// Transparent to the keyboard: the row is one cursor stop and the control
    /// it wraps is what answers. Without this a host would have to know that
    /// "the API key row" is really a SettingField in a slot.
    ///
    /// Resolved on each call rather than bound: `data` is not a bindable
    /// property, so a binding would be evaluated once — before the content
    /// exists — and never update.
    property bool cursor: false
    function control() {
        return controlSlot.data.length > 0 ? controlSlot.data[0] : null;
    }

    onCursorChanged: {
        const c = control();
        if (c && c.cursor !== undefined)
            c.cursor = cursor;
    }
    function keyActivate() {
        const c = control();
        if (c && c.keyActivate)
            c.keyActivate();
    }
    function keyAdjust(delta) {
        const c = control();
        if (c && c.keyAdjust)
            c.keyAdjust(delta);
    }

    StyledText {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.labelWidth
        text: root.label
        color: root.cursor ? Services.Colors.accent : Services.Colors.foreground
        font.pixelSize: 14
        font.bold: root.cursor
        elide: Text.ElideRight
    }

    Item {
        id: controlSlot
        anchors.left: parent.left
        anchors.leftMargin: root.labelWidth + root.gap
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }
}
