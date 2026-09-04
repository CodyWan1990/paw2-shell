import QtQuick

/// Standard scrollable body used by settings tabs, with the keyboard row
/// cursor the launcher drives. The walking lives in KeyNavCursor, which BarTab
/// also uses over its own column; this just supplies the scope and forwards
/// the calls AppLauncher makes.
Flickable {
    id: root

    default property alias content: contentColumn.data
    property int horizontalPadding: 16
    property int verticalPadding: 12
    property int contentSpacing: 10
    property int trailingScrollPadding: 32
    readonly property int rowWidth: width - horizontalPadding * 2

    contentHeight: contentColumn.implicitHeight + trailingScrollPadding
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // ─── keyboard row cursor (see KeyNavCursor) ───
    // Bindings rather than aliases: an alias cannot target a member of an
    // object-typed property, and fails silently enough to look like the whole
    // tab is broken.
    readonly property int cursorIndex: nav.cursorIndex
    readonly property int stopCount: nav.stopCount
    function stepCursor(delta) { return nav.stepCursor(delta); }
    function setCursor(index) { nav.setCursor(index); }
    function activate() { nav.activate(); }
    function adjust(delta) { nav.adjust(delta); }
    function clearCursor() { nav.clearCursor(); }
    function refreshStops() { nav.refresh(); }

    property KeyNavCursor nav: KeyNavCursor {
        scope: contentColumn
        flick: root
    }

    Component.onCompleted: nav.refresh()
    onHeightChanged: nav.ensureVisible()
    // Rows appearing or disappearing changes the stop list; the column's
    // height is the cheapest signal that something did.
    Connections {
        target: contentColumn
        function onImplicitHeightChanged() { nav.refresh(); }
        function onChildrenChanged() { nav.refresh(); }
    }

    Column {
        id: contentColumn
        width: root.width
        topPadding: root.verticalPadding
        bottomPadding: root.verticalPadding
        spacing: root.contentSpacing
    }
}
