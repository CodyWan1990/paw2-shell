import QtQuick
import QtQuick.Layouts
import Quickshell
import "../services" as Services
import "../components" as Components

/// Main status bar — dynamically loads widgets from Settings arrays.
PanelWindow {
    id: bar

    readonly property bool shadowActive: Services.Settings.barShadowEnabled
        && Services.Settings.barShadowRadius > 0
        && Services.Settings.barShadowOpacity > 0
    readonly property int shadowPadding: shadowActive ? Math.ceil(
        Services.Settings.barShadowRadius
        + Math.abs(Services.Settings.barShadowOffsetY)) : 0
    readonly property int contentHeight: Services.Settings.barHeight

    anchors.top: Services.Settings.barPosition === "top"
    anchors.bottom: Services.Settings.barPosition === "bottom"
    anchors.left: true
    anchors.right: true
    implicitHeight: Services.Settings.barHeight + shadowPadding
    exclusiveZone: Services.Settings.barHeight

    // Floating bar — inset from screen edges
    margins.top: Services.Settings.barFloating && Services.Settings.barPosition === "top" ? Services.Settings.barGap : 0
    margins.bottom: Services.Settings.barFloating && Services.Settings.barPosition === "bottom" ? Services.Settings.barGap : 0
    margins.left: Services.Settings.barFloating ? Services.Settings.barGap : 0
    margins.right: Services.Settings.barFloating ? Services.Settings.barGap : 0
    // Always transparent: a Wayland surface's alpha format is fixed at creation,
    // so a window born opaque can never become transparent when radius is raised
    // live. The Rectangle below always draws the (optionally rounded) fill.
    color: "transparent"
    mask: Region { item: barSurface }

    Components.SurfaceShadow {
        anchors.fill: barSurface
        radius: barSurface.radius
        blur: Services.Settings.barShadowRadius
        colorValue: Services.Settings.barShadowColor
        colorOpacity: Services.Settings.barShadowOpacity
        offset: Qt.vector2d(Services.Settings.barShadowOffsetX,
            Services.Settings.barShadowOffsetY)
        visible: bar.shadowActive
        z: -2
    }

    Rectangle {
        id: barSurface
        anchors.left: parent.left
        anchors.right: parent.right
        y: Services.Settings.barPosition === "top" ? 0 : bar.shadowPadding
        height: Services.Settings.barHeight
        radius: Services.Settings.barRadius
        color: Qt.rgba(
            Services.Colors.background.r,
            Services.Colors.background.g,
            Services.Colors.background.b,
            Services.Settings.barOpacity
        )
        z: -1
    }

    // ─── Font (driven by Settings) ───
    property string fontFamily: Services.Settings.fontFamily
    property var fontFamilies: [Services.Settings.fontFamily, "Noto Color Emoji"]
    property int fontSize: Services.Settings.fontSize
    property int smallFontSize: Math.max(10, Services.Settings.fontSize - 3)

    // ─── Widget registry: string ID → QML file path ───
    property var widgetRegistry: ({
        "workspaces": "../widgets/WorkspaceBar.qml",
        "clock": "../widgets/Clock.qml",
        "updates": "../widgets/UpdatesBadge.qml",
        "weather": "../widgets/WeatherWidget.qml",
        "media": "../widgets/MediaPlayer.qml",
        "sysMonitor": "../widgets/SysMonitor.qml",
        "cpu": "../widgets/CpuMonitor.qml",
        "memory": "../widgets/MemoryMonitor.qml",
        "audio": "../widgets/AudioSlider.qml",
        "systemTray": "../widgets/SystemTray.qml",
        "diskUsage": "../widgets/DiskUsage.qml",
        "network": "../widgets/NetworkSpeed.qml",
        "uptime": "../widgets/Uptime.qml",
        "brightness": "../widgets/Brightness.qml",
        "battery": "../widgets/Battery.qml",
        "themeSwitcher": "../widgets/ThemeSwitcher.qml",
        "layout": "../widgets/LayoutSwitcher.qml",
        "emoji": "../widgets/EmojiButton.qml",
        "custom": "../widgets/CustomWidget.qml"
    })

    // ─── Inline components (need TopBar scope) ───
    property var inlineWidgets: ({
        "windowTitle": windowTitleComponent,
        "notifications": notificationsComponent,
        "menu": menuComponent,
        "power": powerComponent,
        "separator": separatorComponent,
        "spacer": spacerComponent
    })

    Component {
        id: windowTitleComponent
        Item {
            id: wtRoot
            property var bar: null
            property var instance: ({})
            // Split icon (RichText, colored) + title (PlainText, elides) so the title
            // truncates with "…" — Qt ignores elide on RichText.
            readonly property int maxTitleWidth: instance.maxWidth ?? 360
            visible: Services.Paw.focusedWindowTitle !== ""
            implicitWidth: wtRow.implicitWidth + 4
            implicitHeight: wtRow.implicitHeight

            Components.BarItem {
                style: instance.style ?? "none"
                accentColor: Services.Colors.byKey(instance.color, Services.Colors.foreground)

                RowLayout {
                    id: wtRow
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    spacing: 4

                    Components.StyledText {
                        id: windowIconText
                        textFormat: Text.RichText
                        text: (wtRoot.instance.prefix ?? "| ") + Services.Icons.markup("window", wtRoot.instance.iconSet ?? "emoji", Services.Colors.byKey(wtRoot.instance.color, Services.Colors.foreground))
                        color: Services.Colors.foreground
                        font.pixelSize: Services.Settings.fontSize
                    }

                    Components.StyledText {
                        id: windowTitleText
                        textFormat: Text.PlainText
                        text: Services.Paw.focusedWindowTitle
                        color: Services.Colors.foreground
                        font.pixelSize: Services.Settings.fontSize
                        elide: Text.ElideRight
                        Layout.maximumWidth: wtRoot.maxTitleWidth
                    }
                }
            }
        }
    }

    Component {
        id: notificationsComponent
        Item {
            property var bar: null
            property var instance: ({})
            visible: !(instance.hideWhenZero ?? false)
                || (Services.NotifManager.notifications.values?.length ?? 0) > 0
            implicitWidth: visible ? notifTextInner.implicitWidth : 0
            implicitHeight: notifTextInner.implicitHeight

            Components.BarItem {
                style: instance.style ?? "underline"
                accentColor: Services.Colors.byKey(instance.color, Services.Colors.notifUnderline)

                Components.StyledText {
                    id: notifTextInner
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.RichText
                    text: {
                        let p = Services.Icons.prefix("notif", instance.iconSet ?? "emoji", Services.Colors.byKey(instance.color, Services.Colors.notifUnderline));
                        let count = Services.NotifManager.notifications.values?.length ?? 0;
                        return (p + (count > 0 ? count : "")).replace(/\s+$/, "");
                    }
                    color: Services.Colors.foreground
                    font.pixelSize: Services.Settings.fontSize
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: controlCenter.isOpen ? controlCenter.close() : controlCenter.open()
                }
            }
        }
    }

    Component {
        id: menuComponent
        Item {
            property var bar: null
            property var instance: ({})
            implicitWidth: menuTextInner.implicitWidth + 12
            implicitHeight: menuTextInner.implicitHeight

            Components.BarItem {
                style: instance.style ?? "underline"
                accentColor: Services.Colors.byKey(instance.color, Services.Colors.menuUnderline)

                Components.StyledText {
                    id: menuTextInner
                    anchors.centerIn: parent
                    text: instance.glyph ?? "\u2630"
                    color: Services.Colors.color8
                    font.pixelSize: Services.Settings.fontSize
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: Services.Actions.launcher()
                }
            }
        }
    }

    Component {
        id: powerComponent
        Item {
            property var bar: null
            property var instance: ({})
            implicitWidth: powerTextInner.implicitWidth + 12
            implicitHeight: powerTextInner.implicitHeight

            Components.BarItem {
                style: instance.style ?? "underline"
                accentColor: Services.Colors.byKey(instance.color, Services.Colors.powermenuUnderline)

                Components.StyledText {
                    id: powerTextInner
                    anchors.centerIn: parent
                    textFormat: Text.RichText
                    text: Services.Icons.markup("power", instance.iconSet ?? "emoji", Services.Colors.byKey(instance.color, Services.Colors.powermenuUnderline))
                    // Honors the per-widget Color setting; red is only the default.
                    color: Services.Colors.byKey(instance.color, Services.Colors.red)
                    font.pixelSize: Services.Settings.fontSize
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: Services.Actions.powermenu()
                }
            }
        }
    }

    Component {
        id: separatorComponent
        Item {
            id: sepRoot
            property var bar: null
            property var instance: ({})
            readonly property string glyph: instance.glyph ?? ""
            implicitWidth: glyph === ""
                ? (instance.width ?? 12)
                : Math.max(instance.width ?? 12, sepText.implicitWidth)
            implicitHeight: glyph === "" ? 1 : sepText.implicitHeight

            Components.StyledText {
                id: sepText
                anchors.centerIn: parent
                visible: sepRoot.glyph !== ""
                text: sepRoot.glyph
                color: Services.Colors.byKey(sepRoot.instance.color, Services.Colors.color8)
                font.pixelSize: Services.Settings.fontSize
            }
        }
    }

    Component {
        id: spacerComponent
        Item {
            property var bar: null
            property var instance: ({})
            implicitWidth: (instance && instance.width !== undefined) ? instance.width : 50
            implicitHeight: 1
        }
    }

    function resolveWidget(type) {
        return inlineWidgets[type] ?? null;
    }

    // ═══ LEFT section ═══
    Components.BarSection {
        id: leftSection
        anchors.left: parent.left
        anchors.top: barSurface.top
        anchors.bottom: barSurface.bottom
        spacing: 4
        model: Services.Settings.barLeft
        hostBar: bar
        registry: bar.widgetRegistry
        resolveComponent: type => bar.resolveWidget(type)
    }

    // ═══ CENTER section (pinned to absolute center) ═══
    Components.BarSection {
        id: centerSection
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: barSurface.top
        anchors.bottom: barSurface.bottom
        spacing: 4
        model: Services.Settings.barCenter
        hostBar: bar
        registry: bar.widgetRegistry
        resolveComponent: type => bar.resolveWidget(type)
    }

    // ═══ RIGHT section ═══
    Components.BarSection {
        id: rightSection
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: barSurface.top
        anchors.bottom: barSurface.bottom
        spacing: 0
        model: Services.Settings.barRight
        hostBar: bar
        registry: bar.widgetRegistry
        resolveComponent: type => bar.resolveWidget(type)
    }

    ControlCenter {
        id: controlCenter
        bar: bar
    }
}
