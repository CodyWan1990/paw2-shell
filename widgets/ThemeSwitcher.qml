import QtQuick
import "../services" as Services
import "../components" as Components

/// Theme switcher bar widget — click opens a dropdown of themes; selecting applies it live.
Components.BarWidget {
    id: root

    icon: "theme"
    defaultAccent: Services.Colors.color5
    clickable: true
    onClicked: themePopup.isOpen ? themePopup.close() : themePopup.open()

    Component.onCompleted: Services.Theme.refresh()

    Components.WidgetPopup {
        id: themePopup
        bar: root.bar
        parentWidget: root
        contentWidth: 220
        contentHeight: Math.min(root.instance.maxPopupHeight ?? 360,
            Services.Theme.themes.length * 34 + 12)
        contentMargins: 6
        accentColor: Services.Colors.color5

        onOpening: Services.Theme.refresh()

        ListView {
            anchors.fill: parent
            clip: true
            model: Services.Theme.themes
            boundsBehavior: Flickable.StopAtBounds

            delegate: Components.InteractiveListRow {
                id: themeRow
                required property var modelData
                width: ListView.view.width
                height: 32
                property bool current: Services.Theme.currentSlug === modelData.slug
                selected: current
                accentColor: Services.Colors.color1
                hoverColor: Services.Colors.color1
                selectedColor: Services.Colors.withAlpha(Services.Colors.color1, 0.2)
                onClicked: {
                    Services.Theme.apply(modelData.slug);
                    themePopup.close();
                }

                Components.StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    text: (themeRow.current ? "✓ " : "") + modelData.name
                    color: themeRow.hovered
                        ? Services.Colors.background
                        : (themeRow.current ? Services.Colors.color1 : Services.Colors.foreground)
                    font.pixelSize: 14
                    font.bold: themeRow.current
                }
            }
        }
    }
}
