import QtQuick
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Quickshell.Io
import "../services" as Services
import "../components" as Components

/// First-run setup — shown once on a machine that has never saved settings,
/// and re-openable from Settings → General or `qs -c paw2-shell ipc call shell
/// welcome`.
///
/// Every page drives an existing service rather than owning state: monitors go
/// through Outputs (wlr-randr → paw's output management → outputs.conf),
/// colours through Theme, the wallpaper through the same apply-wallpaper.sh
/// the launcher uses. Only `firstRunDone` belongs to this panel, and only the
/// Finish button sets it — closing early means "ask me again next login".
Components.OverlayPanel {
    id: welcome

    backdropDim: 0.75
    // Longer than the 60 ms default: on a fresh install this panel is opened
    // from shell.qml as soon as Settings loads, early enough that the focus
    // grab can beat the layer surface being mapped and land on nothing.
    focusDelayMs: 250
    property string fontFamily: Services.Settings.fontFamily

    readonly property var pages: [
        { title: "Monitors", subtitle: "Which screen leads, and how each one runs" },
        { title: "Colours", subtitle: "Pick a theme — it applies as you click" },
        { title: "Wallpaper", subtitle: "Where your images live, and which one to start with" },
        { title: "Apps", subtitle: "Choose apps, a font, and how workspaces tile" },
        { title: "Keys", subtitle: "The bindings you have right now" },
        { title: "Dependencies", subtitle: "What is installed, and what it costs if it isn't" },
        { title: "Done", subtitle: "" }
    ]
    property int page: 0

    /// Keyboard cursor within the current page. Same model as the launcher:
    /// Tab cycles pages, the arrows move this, Enter acts on it.
    property int sel: 0
    onPageChanged: sel = 0

    /// What the arrows walk on this page. Monitors is a flat list of the
    /// screen chips followed by each output's two dropdowns; the two grids
    /// hand back their own models. Pages with nothing to pick return [].
    readonly property var selectables: {
        if (page === 0) return new Array(monitorSetup.selectableCount);
        if (page === 1) return Services.Theme.themes;
        if (page === 2) return wallpaperGrid.count > 0 ? new Array(wallpaperGrid.count) : [];
        if (page === 4) return [];
        return [];
    }

    onOpened: {
        page = 0;
        sel = 0;
        Services.Outputs.refresh();
        Services.Theme.refresh();
        depsCheck.running = true;
        focusScope.forceActiveFocus();
    }
    onFocusRequested: focusScope.forceActiveFocus()

    function next() { page = Math.min(pages.length - 1, page + 1); }
    function back() { page = Math.max(0, page - 1); }

    /// Move the cursor. Grids navigate row-wise through Services.Ui.gridStep;
    /// the Monitors list is linear, so up/left and down/right agree.
    function move(dir) {
        const n = selectables.length;
        // Keys is the one page that is both selectable and scrollable: the two
        // mod-key chips sit on left/right, the binding list on up/down.
        if (page === 4 && (dir === "up" || dir === "down")) {
            keybindList.contentY = Math.max(0, Math.min(
                Math.max(0, keybindList.contentHeight - keybindList.height),
                keybindList.contentY + (dir === "down" ? 60 : -60)));
            return;
        }
        if (n === 0) {
            // Pages that are read-only lists: nothing to select, so the arrows
            // do the only useful thing left and scroll them.
            if (page === 5 && (dir === "up" || dir === "down"))
                depsList.contentY = Math.max(0, Math.min(
                    Math.max(0, depsList.contentHeight - depsList.height),
                    depsList.contentY + (dir === "down" ? 60 : -60)));
            return;
        }
        if (page === 1 || page === 2) {
            const grid = page === 1 ? themeGrid : wallpaperGrid;
            const cols = Math.max(1, Math.floor(grid.width / grid.cellWidth));
            sel = Services.Ui.gridStep(sel, n, cols, dir);
            grid.positionViewAtIndex(sel, GridView.Contain);
            return;
        }
        sel = (dir === "left" || dir === "up") ? Math.max(0, sel - 1)
                                               : Math.min(n - 1, sel + 1);
    }

    /// Enter: act on what the cursor is on, and Finish on the last page.
    function activate() {
        if (page === welcome.pages.length - 1) {
            finish();
            return;
        }
        if (page === 0) {
            monitorSetup.activateAt(sel);
            return;
        }
        const item = selectables[sel];
        if (item === undefined)
            return;
        if (page === 1) {
            Services.Theme.apply(item.slug);
        } else if (page === 2) {
            const entry = wallpaperGrid.itemAtIndex(sel);
            if (entry)
                applyWallpaper(entry.fileName);
        } else if (page === 4) {
            // Nothing selectable: the bindings are shown, not edited here.
        }
    }

    function finish() {
        Services.Settings.firstRunDone = true;
        close();
    }

    // The card lives *inside* the focus scope, not beside it: dropdown filter
    // fields keep their own focus, and keys only reach these handlers by
    // bubbling up through an ancestor. A focused TextInput still consumes
    // Left/Right for its own cursor first, which is what you want.
    FocusScope {
        id: focusScope
        anchors.fill: parent
        focus: true

        // Tab cycles pages, arrows move within one, Enter acts — the model the
        // launcher already uses. Tab needs event.accepted or Qt walks its own
        // focus chain instead.
        Keys.onTabPressed: event => { event.accepted = true; welcome.next(); }
        Keys.onBacktabPressed: event => { event.accepted = true; welcome.back(); }
        Keys.onEscapePressed: welcome.close()
        Keys.onLeftPressed: welcome.move("left")
        Keys.onRightPressed: welcome.move("right")
        Keys.onUpPressed: welcome.move("up")
        Keys.onDownPressed: welcome.move("down")
        Keys.onReturnPressed: welcome.activate()
        Keys.onEnterPressed: welcome.activate()

        Components.OverlaySurface {
            anchors.centerIn: parent
            width: Math.min(920, welcome.width - 80)
            height: Math.min(660, welcome.height - 80)
            opacity: welcome.contentOpacity

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                // ─── Header ───
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.StyledText {
                            text: welcome.page === 0 ? "Welcome to paw2"
                                : welcome.pages[welcome.page].title
                            color: Services.Colors.foreground
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Components.StyledText {
                            text: welcome.pages[welcome.page].subtitle
                            color: Services.Colors.color8
                            font.pixelSize: 13
                            visible: text !== ""
                        }
                    }

                    // Step dots — also clickable, so nothing here is a forced march.
                    Row {
                        spacing: 6
                        Repeater {
                            model: welcome.pages.length
                            delegate: Rectangle {
                                required property int index
                                width: 9; height: 9; radius: 5
                                color: index === welcome.page ? Services.Colors.accent
                                    : (index < welcome.page ? Services.Colors.color8
                                                            : Services.Colors.color0)
                                border.color: Services.Colors.color8
                                border.width: 1
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: welcome.page = parent.index
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Services.Colors.accent
                }

                // ─── Pages ───
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // 0 ── Monitors ────────────────────────────────────────────
                    Flickable {
                        anchors.fill: parent
                        visible: welcome.page === 0
                        contentHeight: monitorSetup.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Components.MonitorSetup {
                            id: monitorSetup
                            width: parent.width
                            // Same component Settings → Monitors mounts; only
                            // this host drives it from the keyboard.
                            cursorIndex: welcome.page === 0 ? welcome.sel : -1
                        }
                    }

                    // 1 ── Colours ─────────────────────────────────────────────
                    GridView {
                        id: themeGrid
                        anchors.fill: parent
                        visible: welcome.page === 1
                        model: Services.Theme.themes
                        cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 210)))
                        cellHeight: 96
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Item {
                            id: themeDelegate
                            required property var modelData
                            required property int index
                            width: GridView.view.cellWidth
                            height: GridView.view.cellHeight

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 6
                                readonly property bool active: themeHover.containsMouse
                                    || Services.Theme.currentSlug === modelData.slug
                                    || (welcome.page === 1 && welcome.sel === themeDelegate.index)
                                radius: Services.Ui.radiusSm
                                color: Services.Colors.color0
                                border.color: active ? Services.Colors.accent : Services.Colors.color8
                                border.width: active ? 2 : 1

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    Components.StyledText {
                                        text: modelData.name
                                        color: Services.Colors.foreground
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }

                                    Row {
                                        spacing: 4
                                        Repeater {
                                            model: (modelData.swatches || []).slice(0, 7)
                                            delegate: Rectangle {
                                                required property var modelData
                                                width: 20; height: 20; radius: 3
                                                color: modelData
                                                border.color: Services.Colors.color8
                                                border.width: 1
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: themeHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Services.Theme.apply(modelData.slug)
                                }
                            }
                        }
                    }

                    // 2 ── Wallpaper ───────────────────────────────────────────
                    ColumnLayout {
                        anchors.fill: parent
                        visible: welcome.page === 2
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Components.StyledText {
                                text: "Folder"
                                color: Services.Colors.foreground
                                font.pixelSize: 14
                                Layout.preferredWidth: 60
                            }
                            Components.SettingField {
                                Component.onCompleted: text = Services.Settings.wallpaperDir
                                onEdited: v => Services.Settings.wallpaperDir = v
                            }
                        }

                        Components.StyledText {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Clicking an image sets it now. With the box below ticked it is "
                                + "also remembered for the theme you just picked, so switching "
                                + "themes later switches the wallpaper with it."
                            color: Services.Colors.color8
                            font.pixelSize: 13
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Components.ChipButton {
                                symbol: Services.Settings.bindWallpaperToTheme ? "☑" : "☐"
                                active: Services.Settings.bindWallpaperToTheme
                                size: 14
                                onClicked: Services.Settings.bindWallpaperToTheme =
                                    !Services.Settings.bindWallpaperToTheme
                            }
                            Components.StyledText {
                                Layout.fillWidth: true
                                text: Services.Theme.currentSlug
                                    ? "Bind to the current theme (" + Services.Theme.currentSlug + ")"
                                    : "Bind to the current theme"
                                color: Services.Settings.bindWallpaperToTheme
                                    ? Services.Colors.foreground : Services.Colors.color8
                                font.pixelSize: 13
                            }
                        }

                        Components.StyledText {
                            Layout.fillWidth: true
                            visible: wallpaperGrid.count === 0
                            wrapMode: Text.WordWrap
                            text: "No images in " + Services.Settings.wallpaperDir
                                + " — point the folder above at your own, or skip this and set "
                                + "one later from the wallpaper picker."
                            color: Services.Colors.color8
                            font.pixelSize: 13
                            font.italic: true
                        }

                        GridView {
                            id: wallpaperGrid
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 200)))
                            cellHeight: 130

                            model: FolderListModel {
                                folder: "file://" + Services.Settings.wallpaperDir
                                nameFilters: ["*.jpg", "*.jpeg", "*.png", "*.webp"]
                                showDirs: false
                                caseSensitive: false
                                sortField: FolderListModel.Name
                            }

                            delegate: Item {
                                id: wpDelegate
                                required property string fileName
                                required property int index
                                width: wallpaperGrid.cellWidth
                                height: wallpaperGrid.cellHeight

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    readonly property bool active: wpHover.containsMouse
                                        || welcome.appliedWallpaper === wpDelegate.fileName
                                        || (welcome.page === 2 && welcome.sel === wpDelegate.index)
                                    color: Services.Colors.color0
                                    border.color: active ? Services.Colors.accent : Services.Colors.color8
                                    border.width: active ? 2 : 1

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        source: "file://" + Services.Settings.wallpaperDir
                                            + "/" + wpDelegate.fileName
                                        sourceSize.width: 280
                                        sourceSize.height: 160
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                    }

                                    MouseArea {
                                        id: wpHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: welcome.applyWallpaper(wpDelegate.fileName)
                                    }
                                }
                            }
                        }
                    }

                    // 4 ── Keys ────────────────────────────────────────────────
                    ColumnLayout {
                        anchors.fill: parent
                        visible: welcome.page === 4
                        spacing: 8

                        Components.SectionHeader { text: "Bindings"; showDivider: false }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Components.StyledText {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: welcome.keybinds.length > 0
                                    ? "Read from paw2's live config. Change any of it in "
                                      + "config.toml, or in Settings → Keybinds — no "
                                      + "recompile, no restart."
                                    : "Not connected to paw2, so there are no bindings to show."
                                color: Services.Colors.color8
                                font.pixelSize: 13
                            }

                            Components.TextButton {
                                text: "Edit"
                                horizontalPadding: 16
                                buttonHeight: 34
                                radius: Services.Ui.radiusSm
                                // Needs an editor chosen on the Apps page first.
                                visible: Services.DefaultApps.editorEntry !== null
                                onClicked: welcome.openInEditor(Services.Paths.keybindsOverlay)
                            }
                        }

                        ListView {
                            id: keybindList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: welcome.keybinds
                            spacing: 2

                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width
                                spacing: 14

                                Components.StyledText {
                                    text: modelData.keys
                                    color: Services.Colors.accent
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 220
                                }
                                Components.StyledText {
                                    text: modelData.action
                                    color: Services.Colors.foreground
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // 3 ── Apps ────────────────────────────────────────────────
                    ColumnLayout {
                        anchors.fill: parent
                        visible: welcome.page === 3
                        spacing: 12

                        Components.StyledDropdown {
                            Layout.fillWidth: true
                            label: "Terminal"
                            labelWidth: 90
                            model: Services.DefaultApps.terminalNames
                            currentValue: Services.DefaultApps.terminalName
                            onSelected: v => Services.DefaultApps.setTerminalByName(v)
                        }

                        Components.StyledDropdown {
                            Layout.fillWidth: true
                            label: "Editor"
                            labelWidth: 90
                            model: Services.DefaultApps.editorNames
                            currentValue: Services.DefaultApps.editorName
                            onSelected: v => Services.DefaultApps.setEditorByName(v)
                        }

                        Components.StyledDropdown {
                            Layout.fillWidth: true
                            label: "UI font"
                            labelWidth: 90
                            filterable: true
                            // Nerd Fonts first — the bar's icons need one.
                            model: Services.Settings.fontFamilies
                            currentValue: Services.Settings.fontFamily
                            onSelected: v => Services.Settings.fontFamily = v
                        }


                        Components.StyledText {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "The terminal is what the shell spawns for upgrades and for "
                                + "its own terminal actions. Your compositor keybind is separate — "
                                + "change it in Settings → Keybinds. The editor is what the Edit "
                                + "buttons open. The workspace layout applies to every workspace; "
                                + "you can customize them individually later in Settings → "
                                + "Compositor."
                            color: Services.Colors.color8
                            font.pixelSize: 13
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 5 ── Dependencies ───────────────────────────────────────
                    ColumnLayout {
                        anchors.fill: parent
                        visible: welcome.page === 5
                        spacing: 8

                        Components.StyledText {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "paw shells out to ordinary command-line tools. A missing one "
                                + "does not break the session — it quietly removes a feature, "
                                + "which is worth knowing about now rather than later."
                            color: Services.Colors.color8
                            font.pixelSize: 13
                        }

                        ListView {
                            id: depsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: welcome.deps
                            spacing: 4

                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width
                                spacing: 12

                                Components.StyledText {
                                    text: modelData.found ? "✓" : "✗"
                                    color: modelData.found ? Services.Colors.green : Services.Colors.red
                                    font.pixelSize: 14
                                    Layout.preferredWidth: 16
                                }
                                Components.StyledText {
                                    text: modelData.name
                                    color: Services.Colors.foreground
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 130
                                }
                                Components.StyledText {
                                    text: modelData.found ? "" : modelData.without
                                    color: Services.Colors.color8
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Components.StyledText {
                            Layout.fillWidth: true
                            visible: welcome.missingDeps.length > 0
                            wrapMode: Text.WordWrap
                            text: "sudo dnf install " + welcome.missingDeps.join(" ")
                            color: Services.Colors.yellow
                            font.pixelSize: 13
                        }
                    }

                    // 6 ── Done ────────────────────────────────────────────────
                    ColumnLayout {
                        anchors.fill: parent
                        visible: welcome.page === 6
                        spacing: 14

                        Item { Layout.fillHeight: true }

                        Components.StyledText {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: "That's the setup."
                            color: Services.Colors.foreground
                            font.pixelSize: 20
                            font.bold: true
                        }

                        Components.StyledText {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: "Everything here lives in Settings too, and this screen is in "
                                + "Settings → General if you want it again.\n\n"
                                + "Finish closes it for good; closing any other way brings it "
                                + "back next login."
                            color: Services.Colors.color8
                            font.pixelSize: 14
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ─── Footer ───
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Components.TextButton {
                        text: "Back"
                        horizontalPadding: 16
                        buttonHeight: 34
                        radius: Services.Ui.radiusSm
                        visible: welcome.page > 0
                        onClicked: welcome.back()
                    }

                    Components.StyledText {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        text: "Tab next  ·  ←→↑↓ select  ·  Enter apply  ·  Esc close"
                        color: Services.Colors.color8
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Components.StyledText {
                        text: (welcome.page + 1) + " / " + welcome.pages.length
                        color: Services.Colors.color8
                        font.pixelSize: 12
                    }

                    Components.TextButton {
                        text: welcome.page === welcome.pages.length - 1 ? "Finish" : "Next"
                        horizontalPadding: 16
                        buttonHeight: 34
                        radius: Services.Ui.radiusSm
                        idleColor: Services.Colors.accent
                        idleBorderColor: Services.Colors.accent
                        idleTextColor: Services.Colors.background
                        onClicked: {
                            if (welcome.page === welcome.pages.length - 1)
                                welcome.finish();
                            else
                                welcome.next();
                        }
                    }
                }
            }
        }
    }


    // ─── Page data ───────────────────────────────────────────────────────
    property string appliedWallpaper: ""

    function applyWallpaper(fileName) {
        appliedWallpaper = fileName;
        wallpaperProc.command = ["bash", Services.Paths.script("apply-wallpaper.sh"),
            Services.Settings.wallpaperDir + "/" + fileName];
        wallpaperProc.running = true;
        // Bare filename, as the launcher stores it, so moving the folder
        // carries every binding with it.
        if (Services.Settings.bindWallpaperToTheme && Services.Theme.currentSlug)
            Services.Settings.setThemeWallpaper(Services.Theme.currentSlug, fileName);
    }

    Process { id: wallpaperProc; running: false }

    /// Open a config file in the chosen editor. argv from DefaultApps, never a
    /// command line built by string concatenation.
    function openInEditor(path) {
        const argv = Services.DefaultApps.editorArgv(path);
        if (argv.length === 0)
            return;
        editorProc.command = argv;
        editorProc.running = true;
    }

    Process { id: editorProc; running: false }

    /// [{ keys, action }] from paw2's live config, flattened for display:
    /// every action with a binding, then every command binding. Read from the
    /// compositor rather than parsed here, so there is no second reader for a
    /// format paw2 has already made sense of.
    readonly property var keybinds: {
        const bindings = Services.PawConfig.get("bindings", {});
        let out = [];
        for (const action of Object.keys(bindings).sort()) {
            // `commands` and `workspace` are tables, not binding lists.
            const keys = bindings[action];
            if (!Array.isArray(keys) || keys.length === 0)
                continue;
            out.push({ keys: keys.join(", "), action: action.replace(/_/g, " ") });
        }
        const commands = bindings.commands ?? {};
        for (const key of Object.keys(commands).sort()) {
            const argv = commands[key];
            if (Array.isArray(argv) && argv.length > 0)
                out.push({ keys: key, action: argv.join(" ") });
        }
        return out;
    }

    /// What the shell shells out to, and what is lost without each. Order is
    /// roughly "most missed first".
    readonly property var depSpecs: [
        // Only needed away from paw2, which reports its own modes over the
        // control socket and takes mode changes through its config instead.
        { name: "wlr-randr", without: "no resolution or refresh control on the Monitors page",
          unlessPaw: true },
        { name: "cliphist", without: "the clipboard picker stays empty" },
        { name: "wl-copy", without: "nothing can be copied from the shell" },
        { name: "wtype", without: "picked emoji are copied but not typed" },
        { name: "grim", without: "screenshots do nothing" },
        { name: "slurp", without: "no region select when taking a screenshot" },
        { name: "swaybg", without: "wallpapers cannot be applied" },
        { name: "jq", without: "the update counter stays at zero" },
        { name: "brightnessctl", without: "the brightness widget cannot change anything" },
        { name: "notify-send", without: "scripts cannot raise notifications" }
    ]
    property var deps: []
    readonly property var missingDeps: deps.filter(d => !d.found).map(d => d.name)

    Process {
        id: depsCheck
        running: false
        command: ["sh", "-c",
            "for c in " + welcome.depSpecs.map(d => d.name).join(" ")
            + "; do command -v \"$c\" >/dev/null 2>&1 && echo \"$c y\" || echo \"$c n\"; done"]
        stdout: StdioCollector {
            onStreamFinished: {
                let found = {};
                for (const line of this.text.trim().split("\n")) {
                    const parts = line.trim().split(" ");
                    if (parts.length === 2)
                        found[parts[0]] = parts[1] === "y";
                }
                // A dependency paw2 makes redundant is not reported missing.
                welcome.deps = welcome.depSpecs
                    .filter(d => !(d.unlessPaw && Services.Paw.connected))
                    .map(d => ({
                        name: d.name, without: d.without, found: !!found[d.name]
                    }));
            }
        }
    }
}
