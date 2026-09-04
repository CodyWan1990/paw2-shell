import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// Package update badge — shows pacman + flatpak count. Click opens package list popup.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.updatesUnderline)
    Layout.fillHeight: true
    implicitWidth: visible ? updatesItem.implicitWidth : 0
    visible: !(instance.hideWhenNone ?? false)
        || Services.Updates.checking || Services.Updates.totalCount > 0

    Components.BarText {
        id: updatesItem
        bar: root.bar
        style: root.instance.style ?? "underline"
        accent: root.accent
        textColor: Services.Updates.statusColor
        clickable: true
        onClicked: {
            if (updatesPopup.isOpen) {
                updatesPopup.close();
            } else {
                updatesPopup.open();
                Services.Updates.refresh();
            }
        }
        text: {
            let p = Services.Icons.prefix("updates", root.instance.iconSet ?? "emoji", root.accent);
            if (Services.Updates.checking) return p + "\u21BB";
            if (Services.Updates.totalCount === 0) return p + "\u2705";
            return p + Services.Updates.totalCount;
        }
    }

    Components.WidgetPopup {
        id: updatesPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 440
        contentHeight: Math.min(650, updCol.implicitHeight + 32)
        accentColor: Services.Colors.updatesUnderline

        Column {
            id: updCol
            width: parent.width
            spacing: 8

            Components.PopupHeader {
                text: Services.Updates.checking
                    ? "\uD83D\uDCE6 Checking..."
                    : Services.Updates.totalCount === 0
                        ? "\uD83D\uDCE6 System Up to Date"
                        : "\uD83D\uDCE6 " + Services.Updates.totalCount + " Updates Available"
                showClose: true
                onCloseClicked: updatesPopup.close()
            }

            Components.StyledText {
                visible: !Services.Updates.checking && Services.Updates.totalCount === 0
                text: "\u2705 All packages are up to date."
                color: Services.Colors.green
                font.pixelSize: 14
            }

            Components.PackageUpdateSection {
                icon: "💻"
                title: "System"
                count: Services.Updates.count
                checking: Services.Updates.checkingSystem
                packages: Services.Updates.packages
                detailWidth: 200
                detailText: packageData => (packageData.current || "")
                    + " → " + (packageData.new || "")
                actionText: "⬆ Upgrade System"
                actionColor: Services.Colors.updatesUnderline
                onActionRequested: {
                    updatesPopup.close();
                    Services.Updates.upgradeSystem();
                }
            }

            Components.Separator { opacity: 1.0 }

            Components.PackageUpdateSection {
                icon: "📦"
                title: "Flatpak"
                count: Services.Updates.flatpakCount
                checking: Services.Updates.checkingFlatpak
                packages: Services.Updates.flatpakPackages
                detailText: packageData => packageData.id || ""
                actionText: "⬆ Update Flatpaks"
                actionColor: Services.Colors.color5
                onActionRequested: {
                    updatesPopup.close();
                    Services.Updates.upgradeFlatpaks();
                }
            }

            Components.Separator { opacity: 1.0 }

            Components.TextButton {
                text: "↻ Refresh All"
                horizontalPadding: 8
                onClicked: Services.Updates.refresh()
            }
        }
    }
}
