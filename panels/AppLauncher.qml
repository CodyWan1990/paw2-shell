import QtQuick
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Quickshell.Io
import "../services" as Services
import "../components" as Components
import "launcher" as LV

/// Unified hub — app launcher, wallpaper picker, and theme switcher in one tabbed panel.
Components.OverlayPanel {
    id: launcher

    onFocusRequested: focusActiveView()

    property string fontFamily: Services.Settings.fontFamily
    property string scriptsDir: Services.Paths.scriptsDir

    // ─── Hub tab state ───
    property string activeView: "apps"
    /// What Tab cycles through. Settings is not here: it is a window of its
    /// own now, and cycling into a tab that immediately closes this panel
    /// would be a strange thing to walk into.
    readonly property var viewList: ["apps", "wallpapers", "themes"]
    readonly property var viewLabels: ["Apps", "Wallpapers", "Themes"]
    readonly property var viewIcons: ["\uf002", "\uf03e", "\uf1fc"]

    // ─── Apps state ─── (filtered list lives in AppsView)
    readonly property var allApps: Services.Applications.entries
    property int appSelectedIndex: 0
    readonly property int appGridColumns: Math.max(1, Math.floor((container.width - 32) / 120))
    readonly property int appCellWidth: Math.floor((container.width - 32) / appGridColumns)
    readonly property int appCellHeight: 110

    // ─── Wallpaper state ─── (filtered list lives in WallpapersView)
    property string wallpaperDir: Services.Settings.wallpaperDir
    property var allWallpapers: []
    property int wpSelectedIndex: 0
    property string applying: ""

    // ─── Themes state ─── (filtered grid model lives in ThemesView)
    property int themeSelectedIndex: 0

    // ─── Theme creator state ───
    property bool creatingTheme: false
    property string newThemeName: ""
    property var newPalette: ({})
    property int activeSlotIndex: 0
    readonly property var themeSlots: [
        { key: "bg",      label: "Background", def: "#1f2430" },
        { key: "fg",      label: "Foreground", def: "#cbccc6" },
        { key: "comment",   label: "Muted",     def: "#5c6773" },
        { key: "selection", label: "Selection", def: "#34455a" },
        { key: "red",     label: "Red",        def: "#f28779" },
        { key: "green",   label: "Green",      def: "#bae67e" },
        { key: "yellow",  label: "Yellow",     def: "#ffd580" },
        { key: "blue",    label: "Blue",       def: "#73d0ff" },
        { key: "mauve",   label: "Purple",     def: "#d4bfff" },
        { key: "cyan",    label: "Cyan",       def: "#95e6cb" }
    ]

    function openCreateTheme() {
        let pal = {};
        for (let slot of themeSlots) pal[slot.key] = slot.def;
        newPalette = pal;
        newThemeName = "";
        activeSlotIndex = 0;
        creatingTheme = true;
    }

    function activeSlotColor() {
        return newPalette[themeSlots[activeSlotIndex].key] || "#000000";
    }

    function setSlotColor(key, hex) {
        let p = {};
        for (let k in newPalette) p[k] = newPalette[k];
        p[key] = hex;
        newPalette = p;
    }

    function saveNewTheme() {
        if (newThemeName.trim() === "") return;
        Services.Theme.saveTheme(newThemeName, newPalette);
        creatingTheme = false;
    }

    // ─── Functions ───
    function focusActiveView() {
        if (activeView === "apps") appsView.focusView();
        else if (activeView === "wallpapers") wallpapersView.focusView();
        else if (activeView === "themes") themesView.focusView();
    }

    function cycleTab(direction) {
        let idx = viewList.indexOf(activeView);
        idx = (idx + direction + viewList.length) % viewList.length;
        activeView = viewList[idx];
        focusActiveView();
    }

    function toggle() {
        toggleTo("apps");
    }

    function toggleTo(view) {
        if (isOpen && activeView === view) {
            close();
            return;
        }
        openTo(view);
    }

    // Base open()/close()/toggle-state come from OverlayPanel (focused-monitor
    // placement, fade, backdrop, focusRequested); this just resets per-view state.
    function openTo(view) {
        activeView = view;
        if (view === "apps") {
            appsView.reset();
            appSelectedIndex = 0;
        } else if (view === "wallpapers") {
            wallpapersView.reset();
            wpSelectedIndex = 0;
            applying = "";
        } else if (view === "themes") {
            themesView.reset();
            Services.Theme.refresh();
            themeSelectedIndex = Math.max(0,
                Services.Theme.themes.findIndex(t => t.slug === Services.Theme.currentSlug));
        }
        open();
    }

    function launchApp(app) {
        if (app && app.desktop) {
            app.desktop.execute();
            close();
        }
    }

    function applyWallpaper(filename) {
        applying = filename;
        applyProc.command = ["bash", scriptsDir + "/apply-wallpaper.sh", wallpaperDir + "/" + filename];
        applyProc.running = true;
        applyTimeout.restart();
        // Stored as a bare filename, so moving the wallpaper directory carries
        // every binding with it.
        if (Services.Settings.bindWallpaperToTheme && Services.Theme.currentSlug)
            Services.Settings.setThemeWallpaper(Services.Theme.currentSlug, filename);
    }

    // ═══ Centered container ═══
    Components.OverlaySurface {
        id: container
        anchors.centerIn: parent
        width: Math.min(1040, parent.width - 32)
        height: Math.min(740, parent.height - 32)
        opacity: launcher.contentOpacity
        Keys.onPressed: event => {
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_S) {
                event.accepted = true;
                Services.Actions.settings();
            }
        }

        Column {
            anchors.fill: parent

            // ═══ Hub Tab Bar ═══
            Rectangle {
                width: parent.width
                height: 44
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 8

                    Repeater {
                        model: launcher.viewList

                        delegate: Components.TabButton {
                            required property string modelData
                            required property int index
                            text: launcher.viewIcons[index] + " " + launcher.viewLabels[index]
                            active: launcher.activeView === modelData
                            onClicked: {
                                launcher.activeView = modelData;
                                launcher.focusActiveView();
                            }
                        }
                    }

                    // A shortcut, not a view. Opening the settings window takes
                    // exclusive keyboard focus, and OverlayPanel.open() closes
                    // whoever held it - so this panel bows out on its own.
                    Components.TabButton {
                        text: "\uf013 Settings"
                        active: false
                        onClicked: Services.Actions.settings()
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: launcher.activeView === "apps"
                            ? "Ctrl+Enter pin · Ctrl+S settings · Tab switch"
                            : "Ctrl+S settings · Tab switch"
                        color: Services.Colors.color8
                        font.family: launcher.fontFamily
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

            // ═══ Content Area ═══
            Item {
                width: parent.width
                height: parent.height - 54   // 46px header (44 tabs + 2 divider) + 8px bottom gap

                // ─── APPS VIEW (extracted to launcher/AppsView.qml) ───
                LV.AppsView {
                    id: appsView
                    anchors.fill: parent
                    visible: launcher.activeView === "apps"
                    launcher: launcher
                }

                // ─── WALLPAPERS VIEW (extracted to launcher/WallpapersView.qml) ───
                LV.WallpapersView {
                    id: wallpapersView
                    anchors.fill: parent
                    visible: launcher.activeView === "wallpapers"
                    launcher: launcher
                }

                // ─── THEMES VIEW (extracted to launcher/ThemesView.qml) ───
                LV.ThemesView {
                    id: themesView
                    anchors.fill: parent
                    visible: launcher.activeView === "themes"
                    launcher: launcher
                }

            }
        }

        // ═══ New-theme creator overlay (extracted to launcher/ThemeBuilder.qml) ═══
        LV.ThemeBuilder {
            anchors.fill: parent
            z: 100
            launcher: launcher
        }
    }

    // ═══ Processes ═══

    // Native directory listing — replaces the old `find | sort` shell-out. Auto-updates
    // when the folder changes; case-insensitive filters + sort match the old `-iname`/`sort -f`.
    FolderListModel {
        id: wallpaperModel
        folder: "file://" + launcher.wallpaperDir
        nameFilters: ["*.jpg", "*.jpeg", "*.png", "*.webp"]
        showDirs: false
        caseSensitive: false
        onStatusChanged: {
            if (status !== FolderListModel.Ready) return;
            let arr = [];
            for (let i = 0; i < count; i++) arr.push(get(i, "fileName"));
            arr.sort((a, b) => a.toLowerCase().localeCompare(b.toLowerCase()));
            launcher.allWallpapers = arr;
        }
    }

    Process {
        id: applyProc
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                // Stay open so you can try multiple wallpapers.
                applyTimeout.stop();
                launcher.applying = "";
            }
        }
    }

    Timer {
        id: applyTimeout
        interval: 30000
        onTriggered: {
            console.warn("Hub: wallpaper apply timed out after 30s");
            launcher.applying = "";
        }
    }

}
