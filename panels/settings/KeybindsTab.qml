import QtQuick
import Quickshell.Io
import "../../services" as Services
import "../../components" as Components

/// Keybinds settings — a raw editor over ~/.config/paw2/config.d/keybinds.toml.
///
/// A drop-in rather than config.toml, for two reasons: the shell never writes
/// the file a user hand-edits, and this file stays clear of the one the rest of
/// the settings UI regenerates, so neither can clobber the other. paw2 applies
/// both over config.toml and reloads within half a second, so hand-editing and
/// this tab remain interchangeable.
///
/// Staying clear of `shell.toml` is load-bearing, not tidiness: a drop-in owns
/// every table it names, and `shell.toml` sorts after this file, so anything
/// that wrote a binding from there would take `[bindings]` away from this tab
/// wholesale. Nothing else in the shell writes bindings, and that is why.
Item {
    id: root
    anchors.fill: parent

    property bool dirty: false
    property bool missing: false
    /// Result of the last reload: "" (none yet), "ok", or "errors".
    property string status: ""
    property var errors: []

    FileView {
        id: conf
        path: Qt.resolvedUrl("file://" + Services.Paths.keybindsOverlay)
        preload: true
        watchChanges: true
        atomicWrites: true

        onLoaded: {
            // Don't clobber unsaved edits when the file changes underneath us.
            if (!root.dirty)
                editor.text = this.text() || "";
            root.missing = false;
        }
        onLoadFailed: root.missing = true
        onFileChanged: reload()
    }

    /// paw2 reports the result of every load, including the one its watcher
    /// starts on its own after this file is written.
    Connections {
        target: Services.Paw
        enabled: root.awaitingReload
        function onConfigResult(ok, errs) {
            root.awaitingReload = false;
            root.status = ok ? "ok" : "errors";
            root.errors = errs;
        }
    }

    /// True between a save and paw2's next config message.
    property bool awaitingReload: false

    function save() {
        conf.setText(editor.text);
        root.dirty = false;
        root.status = "";
        root.errors = [];
        // No reload command: paw2 polls config.d/ twice a second and reports
        // the result over the socket, which is what the Connections above wait
        // for. One path into the config, not two.
        root.awaitingReload = true;
    }

    Column {
        id: col
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Components.SectionHeader { width: col.width; text: "Keybinds"; showDivider: false }

        Components.StyledText {
            width: col.width
            text: "TOML, merged over your config.toml. An action takes a list of "
                + "bindings — <tt>close_window = [\"Super+q\"]</tt> — under "
                + "<tt>[bindings]</tt>; a command takes an argv array under "
                + "<tt>[bindings.commands]</tt>, as "
                + "<tt>\"Super+Return\" = [\"kitty\"]</tt>.<br>"
                + "Modifiers are Super, Ctrl, Alt and Shift; keys are xkb names "
                + "(Return, space, F5, bracketleft…), plus WheelScrollUp and "
                + "WheelScrollDown. '#' starts a comment.<br>"
                + "Listing an action here replaces its bindings entirely. If the file "
                + "does not parse, paw2 keeps the bindings it already had, so a typo "
                + "cannot lock you out."
            textFormat: Text.RichText
            color: Services.Colors.color8
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Components.StyledText {
            width: col.width
            visible: root.missing
            text: "No " + Services.Paths.keybindsOverlay + " yet — paw2 is using the "
                + "bindings from your config.toml. Type overrides below and save."
            color: Services.Colors.yellow
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // ─── Editor ───
        Rectangle {
            width: col.width
            height: col.height - y - saveRow.height - errorBox.height - col.spacing * 3
            radius: 4
            color: Services.Colors.color0
            border.color: editor.activeFocus ? Services.Colors.accent : Services.Colors.color8
            border.width: 1

            Flickable {
                id: editorFlick
                anchors.fill: parent
                anchors.margins: 8
                contentWidth: editor.paintedWidth
                contentHeight: editor.paintedHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                TextEdit {
                    id: editor
                    width: editorFlick.width
                    color: Services.Colors.foreground
                    selectionColor: Services.Colors.accent
                    selectedTextColor: Services.Colors.background
                    font.family: "monospace"
                    font.pixelSize: 13
                    selectByMouse: true
                    textFormat: TextEdit.PlainText
                    tabStopDistance: 32
                    onTextChanged: if (activeFocus) root.dirty = true
                }
            }
        }

        // ─── Save / reload ───
        Row {
            id: saveRow
            width: col.width
            height: 32
            spacing: 10

            Components.TextButton {
                text: root.dirty ? "Save & reload •" : "Save & reload"
                fontSize: 14
                idleBorderColor: root.dirty ? Services.Colors.accent : Services.Colors.color8
                onClicked: root.save()
            }

            Components.TextButton {
                text: "Revert"
                fontSize: 14
                hoverColor: Services.Colors.color1
                onClicked: {
                    root.dirty = false;
                    editor.text = conf.text() || "";
                    root.status = "";
                    root.errors = [];
                }
            }

            Components.StyledText {
                anchors.verticalCenter: parent.verticalCenter
                visible: !Services.Paw.connected
                text: "⚠ not connected to paw2 — saves apply on next start"
                color: Services.Colors.yellow
                font.pixelSize: 12
            }
        }

        // ─── Reload result ───
        Column {
            id: errorBox
            width: col.width
            spacing: 2
            visible: root.status !== ""

            Components.StyledText {
                text: root.status === "ok"
                    ? "✓ Reloaded — all lines parsed."
                    : "⚠ Reloaded with " + root.errors.length + " problem line(s); the rest are active:"
                color: root.status === "ok" ? Services.Colors.green : Services.Colors.red
                font.pixelSize: 12
            }

            Repeater {
                model: root.errors
                delegate: Text {
                    required property var modelData
                    text: "   " + modelData
                    color: Services.Colors.color8
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }
        }
    }
}
