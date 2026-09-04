import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// Screenshot action panel — shows after grim+slurp capture completes.
Components.OverlayPanel {
    id: tool

    onOpened: {
        selectedButton = 0;
        cacheBuster++;
        fileNameInput.text = "";   // blank → date is used as fallback (shown as placeholder)
    }
    onFocusRequested: fileNameInput.forceActiveFocus()

    property string fontFamily: Services.Settings.fontFamily
    // Must match screenshot-capture.sh. Not /tmp — see the comment there.
    property string screenshotPath: (Quickshell.env("XDG_RUNTIME_DIR") || "/tmp")
        + "/paw2-shell-screenshot.png"
    property int selectedButton: 0   // 0=Copy, 1=Save, 2=Both
    property string screenshotsDir: Quickshell.env("HOME") + "/Pictures/Screenshots"
    property int cacheBuster: 0
    property string lastSavedPath: ""   // for the "Open" notification action

    // Default timestamp filename (no .png): date '+%B-%d-%Y-%I-%M-%S-%p'.
    function defaultFilename() {
        return Qt.formatDateTime(new Date(), "MMMM-dd-yyyy-hh-mm-ss-AP");
    }

    function resolvedFilename() {
        let name = fileNameInput.text.trim();
        // This is a filename, not a path: a typed "/" would otherwise mkdir a
        // tree outside screenshotsDir, and "../" would escape it entirely.
        name = name.replace(/[/\\]/g, "-").replace(/^\.+/, "");
        if (name === "") name = defaultFilename();
        if (name.endsWith(".png")) return name;
        return name + ".png";
    }

    function discard() {
        discardProc.command = ["rm", "-f", screenshotPath];
        discardProc.running = true;
        close();
    }

    // Filenames are user-typed — always pass them as argv positionals ("$1"...), never
    // interpolated into the shell string (quote/space injection).
    function copyToClipboard() {
        actionProc.command = ["bash", "-c",
            'wl-copy --type image/png < "$1" && notify-send Screenshot "Copied to clipboard"',
            "bash", screenshotPath];
        actionProc.running = true;
        close();
    }

    function saveToFile() {
        let fname = resolvedFilename();
        lastSavedPath = screenshotsDir + "/" + fname;
        actionProc.command = ["bash", "-c",
            'mkdir -p "$(dirname "$2")" && cp "$1" "$2"' +
            ' && notify-send -A open=Open -A reveal="Show in folder" Screenshot "Saved as $3"',
            "bash", screenshotPath, lastSavedPath, fname];
        actionProc.running = true;
        close();
    }

    function copyAndSave() {
        let fname = resolvedFilename();
        lastSavedPath = screenshotsDir + "/" + fname;
        actionProc.command = ["bash", "-c",
            'wl-copy --type image/png < "$1" && mkdir -p "$(dirname "$2")" && cp "$1" "$2"' +
            ' && notify-send -A open=Open -A reveal="Show in folder" Screenshot "Copied & saved as $3"',
            "bash", screenshotPath, lastSavedPath, fname];
        actionProc.running = true;
        close();
    }

    function executeAction() {
        if (selectedButton === 0) copyToClipboard();
        else if (selectedButton === 1) saveToFile();
        else copyAndSave();
    }

    Components.OverlaySurface {
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 32)
        height: Math.min(440, parent.height - 32)
        opacity: tool.contentOpacity

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Text {
                text: "\uf030  Screenshot"
                color: Services.Colors.foreground
                font.family: tool.fontFamily
                font.pixelSize: 19
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Rectangle { width: parent.width; height: 1; color: Services.Colors.color8 }

            Rectangle {
                width: parent.width
                height: 180
                color: Services.Colors.color0
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 4
                    source: tool.visible ? "file://" + tool.screenshotPath + "?" + tool.cacheBuster : ""
                    fillMode: Image.PreserveAspectFit
                    cache: false
                }
            }

            // ─── Filename input ───
            Rectangle {
                width: parent.width
                height: 36
                color: Services.Colors.color0
                border.color: fileNameInput.activeFocus ? Services.Colors.color4 : Services.Colors.color8
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 0

                    TextInput {
                        id: fileNameInput
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        color: Services.Colors.foreground
                        selectionColor: Services.Colors.color4
                        selectedTextColor: Services.Colors.background
                        font.family: tool.fontFamily
                        font.pixelSize: 15
                        clip: true
                        selectByMouse: true

                        Keys.onEscapePressed: tool.discard()
                        Keys.onReturnPressed: tool.executeAction()
                        Keys.onTabPressed: {
                            tool.selectedButton = (tool.selectedButton + 1) % 3;
                            event.accepted = true;
                        }
                    }

                    Text {
                        text: ".png"
                        color: Services.Colors.color8
                        font.family: tool.fontFamily
                        font.pixelSize: 15
                        Layout.alignment: Qt.AlignVCenter
                        visible: !fileNameInput.text.endsWith(".png")
                    }
                }

                Text {
                    textFormat: Text.PlainText
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: tool.visible ? tool.defaultFilename() : "filename..."
                    color: Services.Colors.color8
                    font.family: tool.fontFamily
                    font.pixelSize: 15
                    visible: fileNameInput.text.length === 0
                }
            }

            // ─── Action buttons ───
            RowLayout {
                width: parent.width
                spacing: 10

                Components.ActionButton {
                    icon: "\uf0c5"; label: "Copy"
                    activeColor: Services.Colors.color4
                    selected: tool.selectedButton === 0
                    onClicked: tool.copyToClipboard()
                    onHovered: tool.selectedButton = 0
                }
                Components.ActionButton {
                    icon: "\uf0c7"; label: "Save"
                    activeColor: Services.Colors.color2
                    selected: tool.selectedButton === 1
                    onClicked: tool.saveToFile()
                    onHovered: tool.selectedButton = 1
                }
                Components.ActionButton {
                    icon: "\uf24d"; label: "Both"
                    activeColor: Services.Colors.color6
                    selected: tool.selectedButton === 2
                    onClicked: tool.copyAndSave()
                    onHovered: tool.selectedButton = 2
                }
            }

            Text {
                text: "Esc: discard  Tab: cycle buttons  Enter: confirm"
                color: Services.Colors.color8
                font.family: tool.fontFamily
                font.pixelSize: 11
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ─── Action processes ───
    // `notify-send -A` stays alive and prints the chosen action key on stdout (our daemon
    // renders the Open / Show-in-folder pills, which invoke it). Route that to xdg-open.
    Process {
        id: actionProc
        command: ["true"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                let key = this.text.trim();
                if (key === "open" && tool.lastSavedPath !== "")
                    Quickshell.execDetached(["xdg-open", tool.lastSavedPath]);
                else if (key === "reveal")
                    Quickshell.execDetached(["xdg-open", tool.screenshotsDir]);
            }
        }
    }
    Process { id: discardProc; command: ["true"]; running: false }
}
