import QtQuick
import Quickshell
import "../services" as Services
import "../components" as Components

/// Toast-style notification popups — anchored top-right, auto-dismiss.
PanelWindow {
    id: toast

    anchors.top: true
    anchors.right: true
    implicitWidth: 400
    implicitHeight: Math.max(1, toastColumn.implicitHeight + 20)
    exclusionMode: ExclusionMode.Ignore
    focusable: false
    color: "transparent"
    margins.top: 40
    margins.right: 10

    // popups, not notifications: those now outlive their toast, and keying the
    // window off them would leave this surface mapped until the user cleared.
    visible: !Services.NotifManager.dndEnabled
        && Services.NotifManager.popups.length > 0

    Column {
        id: toastColumn
        width: parent.width - 20
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Repeater {
            // Already capped at maxVisible and filtered to the unexpired, so
            // membership is the visibility test. An index gate here would mean
            // "the oldest few" now that notifications outlive their toast.
            model: Services.NotifManager.popups

            delegate: Rectangle {
                id: card
                required property var modelData

                width: toastColumn.width
                height: cardContent.implicitHeight + 24
                radius: Services.Ui.radiusMd
                color: Services.Colors.background
                border.color: Services.Colors.notifUnderline
                border.width: 1

                // Slide-down + fade on arrival. (Repeater has no removal
                // transitions, so exit is instant — ponytail.)
                opacity: 0
                transform: Translate { id: slideIn; y: -12 }
                // Once per notification, not once per delegate: the model is
                // rebuilt whenever another arrives or expires, so an
                // unconditional start() would replay this on every toast.
                Component.onCompleted: {
                    if (Services.NotifManager.claimEntry(modelData?.id))
                        enterAnim.start();
                    else
                        opacity = 1;
                }
                ParallelAnimation {
                    id: enterAnim
                    NumberAnimation {
                        target: card; property: "opacity"
                        from: 0; to: 1
                        duration: Services.Ui.durOpen; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: slideIn; property: "y"
                        from: -12; to: 0
                        duration: Services.Ui.durOpen; easing.type: Easing.OutCubic
                    }
                }

                // Click anywhere on the card (except the ✕ / action pills, which sit on top and
                // handle their own clicks) to invoke the default action, if the app supplied one.
                MouseArea {
                    anchors.fill: parent
                    enabled: cardContent.defaultAction !== null
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: cardContent.invokeDefault()
                }

                Components.NotificationContent {
                    id: cardContent
                    anchors.fill: parent
                    anchors.margins: 12
                    notif: card.modelData
                }
            }
        }
    }
}
