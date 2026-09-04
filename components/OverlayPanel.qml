import QtQuick
import Quickshell
import Quickshell.Wayland
import "../services" as Services

/// Shared full-screen overlay-panel chrome: dim backdrop (click-outside to close), exclusive
/// keyboard focus while open, fade in/out, and toggle/open/close with a delayed focus signal.
///
/// Subclasses add their content as child items — declared after this base's backdrop, so they
/// render above it — and bind `opacity: <panel>.contentOpacity` on their content root so it
/// fades with the backdrop. Reset per-open state in `onOpened` (fired synchronously at open());
/// focus the input in `onFocusRequested` (fired focusDelayMs later, once the surface is mapped).
PanelWindow {
    id: panel

    property int focusDelayMs: 60
    /// Backdrop dim strength (0..1) at full fade-in.
    property real backdropDim: 0.5
    /// Open on the compositor-focused monitor. Per-screen panels (one instance per bar) set false.
    property bool followCompositorFocus: true
    /// Logical open state — true from open() until close(); visible stays true
    /// slightly longer so the fade-out can play.
    property bool isOpen: false
    /// Bind content root opacity to this; the backdrop alpha follows it too.
    property real contentOpacity: 0

    signal opened()
    signal focusRequested()

    anchors.top: true
    anchors.bottom: true
    anchors.left: true
    anchors.right: true
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.keyboardFocus: isOpen ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    visible: false
    color: Qt.rgba(0, 0, 0, backdropDim * contentOpacity)

    Behavior on contentOpacity {
        NumberAnimation {
            duration: panel.isOpen ? Services.Ui.durOpen : Services.Ui.durClose
            easing.type: Easing.OutCubic
        }
    }

    function toggle() { if (isOpen) close(); else open(); }

    function open() {
        // Only one panel may hold exclusive keyboard focus. paw focuses the
        // *topmost* keyboard-interactive layer surface (arrangelayers() in
        // paw.c), so with two open the one that opened first can keep the
        // keys and the new panel renders deaf — which is what opening the
        // welcome screen from Settings used to do.
        if (Services.Ui.exclusivePanel && Services.Ui.exclusivePanel !== panel)
            Services.Ui.exclusivePanel.close();
        Services.Ui.exclusivePanel = panel;

        // Follow compositor focus so the overlay opens on the monitor in use (set before mapping).
        if (followCompositorFocus && Services.Paw.focusedScreen) screen = Services.Paw.focusedScreen;
        isOpen = true;
        opened();
        visible = true;
        contentOpacity = 1;
        focusDelay.start();
    }

    function close() {
        // Conditional: another panel may already have claimed focus, and a
        // late close must not clear its claim.
        if (Services.Ui.exclusivePanel === panel)
            Services.Ui.exclusivePanel = null;
        isOpen = false;
        contentOpacity = 0;
        hideDelay.restart();
    }

    Timer { id: focusDelay; interval: panel.focusDelayMs; onTriggered: panel.focusRequested() }

    // Unmap after the fade-out; a reopen mid-fade leaves isOpen true so this no-ops.
    Timer {
        id: hideDelay
        interval: Services.Ui.durClose
        onTriggered: if (!panel.isOpen) panel.visible = false
    }

    MouseArea { anchors.fill: parent; onClicked: panel.close() }
}
