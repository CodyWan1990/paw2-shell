import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Bordered single-line text field used across the settings tabs.
Rectangle {
    id: field

    property alias text: input.text
    property bool password: false
    /// Set by a host whose keyboard cursor is on this row.
    property bool cursor: false
    signal edited(string value)
    /// Fires per keystroke — for live UI state (e.g. enabling a save button).
    /// Don't wire this to Settings writes; use edited() (editingFinished) for those.
    signal liveEdited(string value)

    /// Keyboard surface (see SettingsPage). Enter starts editing; while the
    /// TextInput holds focus the arrows are its own, which is why the host's
    /// row cursor stops moving until Esc or Enter gives the keyboard back.
    function keyActivate() { input.forceActiveFocus(); }

    Layout.fillWidth: true
    height: 32
    radius: 4
    color: Services.Colors.color0
    border.color: input.activeFocus || field.cursor ? Services.Colors.accent
                                                    : Services.Colors.color8
    border.width: input.activeFocus || field.cursor ? 2 : 1

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        color: Services.Colors.foreground
        selectionColor: Services.Colors.accent
        selectedTextColor: Services.Colors.background
        font.family: Services.Settings.fontFamily
        font.pixelSize: 14
        clip: true
        selectByMouse: true
        echoMode: field.password ? TextInput.Password : TextInput.Normal
        onEditingFinished: field.edited(text)
        onTextChanged: field.liveEdited(text)

        // Esc abandons the keyboard without committing; Enter commits first
        // (onEditingFinished fires on accept). Either way focus goes back up to
        // the enclosing FocusScope, so the arrows return to the host's row
        // cursor and Esc reads as "leave the field", not "close the panel".
        Keys.onEscapePressed: event => {
            event.accepted = true;
            input.focus = false;
        }
        onAccepted: input.focus = false
    }
}
