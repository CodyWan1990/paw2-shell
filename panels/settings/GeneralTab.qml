import QtQuick
import "../../services" as Services
import "../../components" as Components

/// General settings — fonts for the bar and panels, and where wallpapers live.
Components.SettingsPage {
    id: root
    anchors.fill: parent

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "General"; showDivider: false }

    Components.StyledDropdown {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Font"
        model: Services.Settings.fontFamilies
        filterable: true
        currentValue: Services.Settings.fontFamily
        onSelected: value => Services.Settings.fontFamily = value
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Font size"; from: 10; to: 28; stepSize: 1; suffix: "px"
        value: Services.Settings.fontSize
        onMoved: v => Services.Settings.fontSize = Math.round(v)
    }

    Components.LabeledSettingRow {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Wallpapers"

        Components.SettingField {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            Component.onCompleted: text = Services.Settings.wallpaperDir
            onEdited: v => Services.Settings.wallpaperDir = v
        }
    }

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Setup" }

    Components.LabeledSettingRow {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Welcome screen"

        Components.TextButton {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "Run setup again"
            onClicked: Services.Actions.welcome()
        }
    }
}
