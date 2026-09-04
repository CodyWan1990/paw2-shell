import QtQuick
import "../services" as Services

/// One section row in the settings sidebar.
///
/// Exposes `cursor` + `keyActivate()`, which is the whole leaf contract
/// KeyNavCursor looks for — so the sidebar is walked by the same code the
/// settings rows use, and the group headers between the rows need no special
/// case: plain text has no `keyActivate`, so it is simply not a stop.
Rectangle {
    id: item

    property string label: ""
    /// The section currently showing in the page area.
    property bool active: false
    /// Set by KeyNavCursor when the keyboard cursor is on this row.
    property bool cursor: false
    signal clicked()

    function keyActivate() { item.clicked(); }

    width: parent ? parent.width : 0
    height: 32
    radius: 6
    color: active
        ? Services.Colors.accent
        : (hover.containsMouse ? Services.Colors.accentAlpha(0.2) : "transparent")
    // Only when it is not the active row: an outline on the accent fill reads
    // as a rendering artefact rather than a cursor.
    border.color: Services.Colors.accent
    border.width: cursor && !active ? 2 : 0

    StyledText {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 8
        text: item.label
        color: item.active ? Services.Colors.background : Services.Colors.foreground
        font.pixelSize: 13
        font.bold: item.active
        elide: Text.ElideRight
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: item.clicked()
    }
}
