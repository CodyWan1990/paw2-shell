import QtQuick
import "../services" as Services
import "../components" as Components

/// Settings — its own window, not a launcher tab.
///
/// Half the screen wide and full height, with a grouped sidebar. The tab files
/// under settings/ are unchanged and unaware of this: each is a SettingsPage
/// carrying its own row cursor, and this only has to choose which one is
/// loaded and hand it the keyboard.
///
/// The sidebar's own cursor is a second KeyNavCursor over the nav column, which
/// is why there is no navigation code here beyond routing keys to one of the
/// two.
Components.OverlayPanel {
    id: settings

    onFocusRequested: {
        nav.setCursor(settings.section);
        keys.forceActiveFocus();
    }
    // Settings writes are debounced; closing inside that window would otherwise
    // drop the last edit. No-op when nothing is pending.
    onIsOpenChanged: if (!isOpen) Services.PawConfig.flush()

    /// Index into `sections`.
    property int section: 0
    /// False once the keyboard has stepped into the page body.
    property bool onSidebar: true

    /// `group` starts a new heading; `paw` marks a section that only means
    /// anything with paw2 behind it. Grouped the way the config is owned: the
    /// first group writes the shell's settings.json, the second writes paw2's
    /// config.d/.
    readonly property var allSections: [
        { group: "SHELL",      label: "Bar",      source: "settings/BarTab.qml" },
        { label: "Dock",     source: "settings/DockTab.qml" },
        { label: "General",  source: "settings/GeneralTab.qml" },
        { label: "Apps",     source: "settings/AppsTab.qml" },
        { label: "Weather",  source: "settings/WeatherTab.qml" },
        // "Display" rather than "Compositor": the group is already called that.
        { group: "COMPOSITOR", label: "Monitors", source: "settings/MonitorsTab.qml", paw: true },
        { label: "Display",  source: "settings/CompositorTab.qml", paw: true },
        { label: "Keybinds", source: "settings/KeybindsTab.qml", paw: true }
    ]
    /// On another compositor the second group drops out entirely — its heading
    /// rides on its first entry, so it goes with them.
    readonly property var sections: allSections.filter(
        s => Services.Paw.connected || !s.paw)

    /// The loaded page, when it implements the row-cursor contract. KeybindsTab
    /// is a raw text editor and deliberately does not, so every caller guards.
    readonly property var page: pageLoader.item
        && pageLoader.item.stepCursor !== undefined ? pageLoader.item : null

    /// Moving the sidebar cursor *is* choosing the section — the page follows
    /// the cursor rather than waiting for Enter, so arrowing down the sidebar
    /// browses. Enter is then free to mean "step into the page".
    property Connections navConn: Connections {
        target: nav
        function onCursorIndexChanged() {
            if (nav.cursorIndex >= 0)
                settings.section = nav.cursorIndex;
        }
    }

    function selectSection(index) {
        const n = sections.length;
        if (n === 0)
            return;
        nav.setCursor(((index % n) + n) % n);
        onSidebar = true;
        if (page)
            page.clearCursor();
    }

    function enterPage() {
        if (!page)
            return;
        onSidebar = false;
        page.stepCursor(1);
    }

    function leavePage() {
        onSidebar = true;
        if (page)
            page.clearCursor();
        nav.setCursor(section);
    }

    Components.OverlaySurface {
        id: container
        anchors.centerIn: parent
        // Half the screen, with a floor so it stays usable on a small display
        // and a margin so it never touches the edges.
        width: Math.min(Math.max(parent.width * 0.5, 720), parent.width - 32)
        height: parent.height - 48
        opacity: settings.contentOpacity

        FocusScope {
            id: keys
            anchors.fill: parent
            focus: true

            Keys.onEscapePressed: settings.close()
            // Tab is the sidebar/page toggle. In a window of its own there is
            // nothing else for it to do, which is what makes it unambiguous -
            // Left already means "adjust this row".
            Keys.onTabPressed: event => {
                event.accepted = true;
                if (settings.onSidebar)
                    settings.enterPage();
                else
                    settings.leavePage();
            }
            Keys.onBacktabPressed: event => {
                event.accepted = true;
                if (settings.onSidebar)
                    settings.enterPage();
                else
                    settings.leavePage();
            }

            Keys.onUpPressed: event => {
                if (event.modifiers & Qt.ControlModifier) {
                    settings.selectSection(settings.section - 1);
                    return;
                }
                if (settings.onSidebar || !settings.page) {
                    nav.stepCursor(-1);
                    return;
                }
                // Running off the top of the page walks back out to the
                // sidebar, the same gesture that used to park on the tab row.
                if (!settings.page.stepCursor(-1))
                    settings.leavePage();
            }
            Keys.onDownPressed: event => {
                if (event.modifiers & Qt.ControlModifier) {
                    settings.selectSection(settings.section + 1);
                    return;
                }
                if (settings.onSidebar || !settings.page)
                    nav.stepCursor(1);
                else
                    settings.page.stepCursor(1);
            }
            Keys.onLeftPressed: {
                if (!settings.onSidebar && settings.page)
                    settings.page.adjust(-1);
            }
            Keys.onRightPressed: {
                if (settings.onSidebar)
                    settings.enterPage();
                else if (settings.page)
                    settings.page.adjust(1);
            }
            Keys.onReturnPressed: settings.onSidebar
                ? settings.enterPage() : (settings.page && settings.page.activate())
            Keys.onEnterPressed: settings.onSidebar
                ? settings.enterPage() : (settings.page && settings.page.activate())

            Column {
                anchors.fill: parent

                // ═══ Title bar ═══
                Item {
                    width: parent.width
                    height: 48

                    Components.StyledText {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Settings"
                        color: Services.Colors.foreground
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Components.StyledText {
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "↑↓ move · Tab in/out · ←→ change · ^↑↓ section"
                        color: Services.Colors.color8
                        font.pixelSize: 11
                    }
                }

                Rectangle { width: parent.width; height: 2; color: Services.Colors.accent }

                // ═══ Sidebar + page ═══
                Item {
                    width: parent.width
                    height: parent.height - 50

                    Flickable {
                        id: navFlick
                        width: 200
                        height: parent.height
                        contentHeight: navColumn.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Column {
                            id: navColumn
                            width: navFlick.width
                            topPadding: 10
                            bottomPadding: 10
                            spacing: 2

                            Repeater {
                                model: settings.sections

                                // A Column per entry so a group heading can ride
                                // on the row that starts the group: hiding the
                                // row hides its heading, with no second model.
                                delegate: Column {
                                    id: entry
                                    required property var modelData
                                    required property int index
                                    width: navColumn.width - 16
                                    x: 8
                                    spacing: 4

                                    Components.StyledText {
                                        visible: !!entry.modelData.group
                                        text: entry.modelData.group ?? ""
                                        color: Services.Colors.color8
                                        font.pixelSize: 10
                                        font.bold: true
                                        leftPadding: 12
                                        topPadding: entry.index === 0 ? 0 : 12
                                        bottomPadding: 2
                                    }

                                    Components.SettingsNavItem {
                                        label: entry.modelData.label
                                        active: settings.section === entry.index
                                        onClicked: settings.selectSection(entry.index)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        x: navFlick.width
                        width: 1
                        height: parent.height
                        color: Services.Colors.color8
                        opacity: 0.3
                    }

                    Loader {
                        id: pageLoader
                        x: navFlick.width + 1
                        width: parent.width - navFlick.width - 1
                        height: parent.height
                        source: settings.isOpen && settings.section < settings.sections.length
                            ? settings.sections[settings.section].source
                            : ""
                        // A freshly loaded page has no cursor; if the keyboard
                        // is already in the body, put it on the first row.
                        onLoaded: if (!settings.onSidebar) settings.enterPage()
                    }
                }
            }
        }

        /// The sidebar's row cursor. Same walker the settings rows use — the
        /// nav items expose `cursor` + `keyActivate()`, the headings do not.
        property Components.KeyNavCursor nav: Components.KeyNavCursor {
            id: nav
            scope: navColumn
            flick: navFlick
        }

        Component.onCompleted: nav.refresh()
        // Sections appear and vanish when paw2 connects, which changes the stop
        // list; the column's height is the cheapest signal that it did.
        Connections {
            target: navColumn
            function onImplicitHeightChanged() { nav.refresh(); }
            function onChildrenChanged() { nav.refresh(); }
        }
    }
}
