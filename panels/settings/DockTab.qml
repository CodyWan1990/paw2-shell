import QtQuick
import "../../services" as Services
import "../../components" as Components

/// Dock settings. All controls use the shared keyboard row contract.
Components.SettingsPage {
    id: root
    anchors.fill: parent

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Behavior"; showDivider: false }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Enabled"; model: ["off", "on"]
        currentValue: Services.Settings.dockEnabled ? "on" : "off"
        onSelected: value => Services.Settings.dockEnabled = value === "on"
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Visibility"; model: ["reserved", "autohide"]
        currentValue: Services.Settings.dockMode
        onSelected: value => Services.Settings.dockMode = value
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Position"; model: ["bottom", "left", "right"]
        currentValue: Services.Settings.dockPosition
        onSelected: value => Services.Settings.dockPosition = value
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled && Services.Settings.dockMode === "autohide"
        label: "Hide delay"; from: 100; to: 1500; stepSize: 100; suffix: "ms"
        value: Services.Settings.dockHideDelayMs
        onMoved: v => Services.Settings.dockHideDelayMs = Math.round(v)
    }

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Appearance" }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Icon size"; from: 32; to: 64; stepSize: 4; suffix: "px"
        value: Services.Settings.dockIconSize
        onMoved: v => Services.Settings.dockIconSize = Math.round(v)
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Edge margin"; from: 0; to: 32; stepSize: 2; suffix: "px"
        value: Services.Settings.dockEdgeMargin
        onMoved: v => Services.Settings.dockEdgeMargin = Math.round(v)
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Opacity"; from: 0.2; to: 1.0; stepSize: 0.05; decimals: 2
        value: Services.Settings.dockOpacity
        onMoved: v => Services.Settings.dockOpacity = v
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Radius"; from: 0; to: 40; stepSize: 2; suffix: "px"
        value: Services.Settings.dockRadius
        onMoved: v => Services.Settings.dockRadius = Math.round(v)
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Outline"; model: ["off", "on"]
        currentValue: Services.Settings.dockBorderEnabled ? "on" : "off"
        onSelected: value => Services.Settings.dockBorderEnabled = value === "on"
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Shadow"; model: ["off", "on"]
        currentValue: Services.Settings.dockShadowEnabled ? "on" : "off"
        onSelected: value => Services.Settings.dockShadowEnabled = value === "on"
    }

    Components.ShadowSettings {
        x: root.horizontalPadding; width: root.rowWidth
        target: "dock"
        enabled: Services.Settings.dockEnabled && Services.Settings.dockShadowEnabled
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Magnify"; model: ["off", "on"]
        currentValue: Services.Settings.dockMagnificationEnabled ? "on" : "off"
        onSelected: value => Services.Settings.dockMagnificationEnabled = value === "on"
    }

    Components.StyledSlider {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled && Services.Settings.dockMagnificationEnabled
        label: "Magnification"; from: 1.05; to: 1.35; stepSize: 0.05; decimals: 2; suffix: "×"
        value: Services.Settings.dockMagnification
        onMoved: v => Services.Settings.dockMagnification = v
    }

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Content" }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Launcher"; model: ["off", "on"]
        currentValue: Services.Settings.dockShowLauncher ? "on" : "off"
        onSelected: value => Services.Settings.dockShowLauncher = value === "on"
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Running apps"; model: ["off", "on"]
        currentValue: Services.Settings.dockShowRunningApps ? "on" : "off"
        onSelected: value => Services.Settings.dockShowRunningApps = value === "on"
    }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        enabled: Services.Settings.dockEnabled
        label: "Indicators"; model: ["off", "on"]
        currentValue: Services.Settings.dockShowIndicators ? "on" : "off"
        onSelected: value => Services.Settings.dockShowIndicators = value === "on"
    }
}
