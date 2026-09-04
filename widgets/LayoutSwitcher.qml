import QtQuick
import "../services" as Services
import "../components" as Components

/// Layout switcher bar widget — shows *this* monitor's tiling layout; click
/// opens the list, scroll cycles. Backed by paw2's set-layout command.
Components.BarWidget {
    id: root

    icon: "layout"
    defaultAccent: Services.Colors.color4
    clickable: true
    onClicked: layoutPopup.isOpen ? layoutPopup.close() : layoutPopup.open()
    /// Hidden without paw2: nothing else can report or change the layout.
    visible: Services.Paw.connected

    /// The bar this widget sits on is per-monitor, so read and write that
    /// monitor's layout rather than the focused one — same reasoning, and the
    /// same `bar.screen.name` source, as WorkspaceBar.
    readonly property string outputName: bar && bar.screen ? bar.screen.name : ""
    readonly property string layoutName: Services.Paw.layoutFor(outputName)
    readonly property int layoutIndex: Services.Paw.layoutIndexFor(outputName)

    text: {
        const name = root.layoutName;
        if (!name)
            return "…";
        return (instance.display ?? "symbol") === "name" ? label(name) : (symbols[name] ?? name);
    }

    /// Compact glyphs for paw2's tiling layouts; an unknown name shows as-is.
    readonly property var symbols: ({
        "scrolling": "[S]",
        "dwindle": "[\\]",
        "master_stack": "[]=",
        "monocle": "[M]"
    })

    /// "master_stack" reads badly in a bar; everything else just needs a capital.
    function label(name) {
        return name.split("_").map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(" ");
    }

    // Scroll over the widget to step through layouts without opening the list.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: wheel => {
            const n = Services.Paw.layouts.length;
            if (n === 0)
                return;
            const step = wheel.angleDelta.y < 0 ? 1 : -1;
            const next = (root.layoutIndex + step + n) % n;
            Services.Paw.setLayout(Services.Paw.layouts[next], root.outputName);
        }
    }

    Components.WidgetPopup {
        id: layoutPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 200
        contentHeight: Math.min(300, Services.Paw.layouts.length * 34 + 12)
        contentMargins: 6
        accentColor: root.accent

        ListView {
            anchors.fill: parent
            clip: true
            model: Services.Paw.layouts
            boundsBehavior: Flickable.StopAtBounds

            delegate: Components.InteractiveListRow {
                id: layoutRow
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 32
                readonly property bool current: root.layoutIndex === index
                selected: current
                accentColor: root.accent
                hoverColor: root.accent
                selectedColor: Services.Colors.withAlpha(root.accent, 0.2)
                onClicked: {
                    Services.Paw.setLayout(layoutRow.modelData, root.outputName);
                    layoutPopup.close();
                }

                Components.StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    text: (layoutRow.current ? "✓ " : "") + root.label(layoutRow.modelData)
                    color: layoutRow.hovered
                        ? Services.Colors.background
                        : (layoutRow.current ? root.accent : Services.Colors.foreground)
                    font.pixelSize: 14
                    font.bold: layoutRow.current
                }

                Components.StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    text: root.symbols[layoutRow.modelData] ?? ""
                    color: layoutRow.hovered
                        ? Services.Colors.background
                        : Services.Colors.color8
                    font.pixelSize: 12
                }
            }
        }
    }
}
