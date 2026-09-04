import QtQuick
import Quickshell
import Quickshell.Wayland
import "../services" as Services

/// Reusable popup window for bar widgets — PanelWindow with fade, backdrop click-to-close, Escape, and positioning relative to the parent widget.
PanelWindow {
    id: popup

    required property var bar
    required property Item parentWidget

    property int contentWidth: 320
    property color accentColor: Services.Colors.color1
    property int contentHeight: -1  // -1 = auto from children
    property int contentMargins: 16
    property int cornerRadius: Services.Ui.radiusMd

    property int fadeInDuration: Services.Ui.durOpen
    property int fadeOutDuration: Services.Ui.durClose
    property bool useOpenDelay: false

    property bool alignLeft: false
    property int xOffset: 0
    property int xClamp: 8

    property bool isOpen: false
    signal opening()

    default property alias contentData: innerContent.data

    screen: bar?.screen ?? null
    anchors.top: true
    anchors.bottom: true
    anchors.left: true
    anchors.right: true
    exclusionMode: ExclusionMode.Ignore
    // isOpen, not visible: visible stays true through the whole fade-out, and a
    // surface that is still holding the seat when it goes away strands the
    // compositor's exclusive focus. OverlayPanel does the same.
    WlrLayershell.keyboardFocus: isOpen ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    visible: false
    color: "transparent"

    // Any bar-layout edit resets TopBar's Repeaters, so a widget can be destroyed
    // out from under its own popup. Unmap first rather than letting a mapped,
    // focus-holding layer surface be torn down.
    Component.onDestruction: popup.visible = false

    NumberAnimation {
        id: fadeIn
        target: contentRect; property: "opacity"
        from: 0.0; to: 1.0; duration: popup.fadeInDuration
        easing.type: Easing.OutCubic
    }
    NumberAnimation {
        id: fadeOut
        target: contentRect; property: "opacity"
        from: 1.0; to: 0.0; duration: popup.fadeOutDuration
        easing.type: Easing.InCubic
        // Guard: a reopen during the fade-out must not blank the popup when it lands.
        onFinished: if (!popup.isOpen) popup.visible = false
    }

    Timer {
        id: openDelay
        interval: 60
        onTriggered: fadeIn.start()
    }

    function open() {
        if (!parentWidget) return;   // widget destroyed under us mid-rebuild
        let pos = parentWidget.mapToItem(null, 0, 0);
        if (alignLeft) {
            contentRect.x = Math.max(xClamp, pos.x + xOffset);
        } else {
            contentRect.x = Math.max(xClamp,
                pos.x - contentRect.width / 2 + parentWidget.width / 2 + xOffset);
        }
        contentRect.opacity = 0;
        visible = true;
        isOpen = true;
        opening();
        if (useOpenDelay) {
            openDelay.start();
        } else {
            fadeIn.start();
        }
    }

    function close() {
        isOpen = false;
        fadeOut.start();
    }

    MouseArea {
        anchors.fill: parent
        onClicked: popup.close()
    }

    FocusScope {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: popup.close()
    }

    Rectangle {
        id: contentRect
        y: popup.bar?.implicitHeight ?? 35
        width: popup.contentWidth
        height: popup.contentHeight >= 0
            ? popup.contentHeight
            : innerContent.childrenRect.height + popup.contentMargins * 2
        color: Services.Colors.background
        border.color: popup.accentColor
        border.width: 1
        radius: popup.cornerRadius
        opacity: 0

        Behavior on height {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        // Block clicks from passing through to backdrop
        MouseArea { anchors.fill: parent }

        Item {
            id: innerContent
            anchors.fill: parent
            anchors.margins: popup.contentMargins
        }
    }
}
