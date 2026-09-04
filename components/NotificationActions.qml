import QtQuick
import "../services" as Services

/// Row of pill buttons for a notification's freedesktop actions. Excludes the "default"
/// action (that's the click-the-body action, handled by the surrounding delegate).
/// invoke() activates the action and dismisses the notification (unless it's resident).
Flow {
    id: root

    property var notif: null
    readonly property var actionList: (notif && notif.actions ? notif.actions : [])
        .filter(a => a.identifier !== "default")

    spacing: 6
    visible: actionList.length > 0

    Repeater {
        model: root.actionList

        delegate: TextButton {
            required property var modelData   // a NotificationAction
            text: modelData.text || modelData.identifier
            horizontalPadding: 10
            buttonHeight: 26
            hoverBorderColor: Services.Colors.color8
            onClicked: modelData.invoke()
        }
    }
}
