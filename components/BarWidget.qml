import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Skeleton shared by the single-label bar widgets (cpu, memory, disk, network,
/// uptime, theme, layout). Each of those was repeating the same twelve lines:
/// the `bar`/`instance` properties the TopBar loader assigns, the palette-key
/// lookup for the accent, the fillHeight/implicitWidth pair, and a BarText fed
/// `instance.style` plus an icon prefix. A widget now declares only what is
/// actually its own — its glyph, its default accent, and its label text.
///
/// Extra children (popups, wheel handlers) are just declared inside the
/// instance; they land in `data` alongside the BarText like any Item child.
Item {
    id: root

    /// Set by TopBar's Loader — the bar this widget lives on, and the widget
    /// instance object from settings.json ({ id, type, style, color, iconSet }).
    property var bar: null
    property var instance: ({})

    /// Icons.qml key for the leading glyph; "" for no icon.
    property string icon: ""
    /// Accent used when the instance's `color` is "auto" or unrecognised.
    property color defaultAccent: Services.Colors.foreground
    /// Label text, appended after the icon. Rich text — callers colour spans.
    property string text: ""
    property bool clickable: false
    signal clicked()

    readonly property color accent: Services.Colors.byKey(instance.color, defaultAccent)

    Layout.fillHeight: true
    implicitWidth: body.implicitWidth

    BarText {
        id: body
        bar: root.bar
        style: root.instance.style ?? "underline"
        accent: root.accent
        clickable: root.clickable
        onClicked: root.clicked()
        // Icon-only widgets use markup(), not prefix(), so they get no trailing
        // space padding the label out.
        text: root.text === ""
            ? Services.Icons.markup(root.icon, root.instance.iconSet ?? "emoji", root.accent)
            : Services.Icons.prefix(root.icon, root.instance.iconSet ?? "emoji", root.accent)
              + root.text
    }
}
