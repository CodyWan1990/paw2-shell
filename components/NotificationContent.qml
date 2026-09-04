import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Security-hardened notification fields and actions shared by cards and toasts.
Column {
    id: root

    property var notif: null
    property int summarySize: 15
    property int bodySize: 14
    property int bodyMaximumLines: 4
    property int contentSpacing: 4
    readonly property var defaultAction:
        (notif && notif.actions ? notif.actions : [])
            .find(action => action.identifier === "default") || null

    spacing: contentSpacing

    function invokeDefault() {
        if (defaultAction) defaultAction.invoke();
    }

    RowLayout {
        width: parent.width

        StyledText {
            text: root.notif?.appName || "Notification"
            color: Services.Colors.color8
            font.pixelSize: 12
            Layout.fillWidth: true
        }

        Text {
            text: "×"
            color: Services.Colors.color8
            font.pixelSize: 13
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.notif?.dismiss()
            }
        }
    }

    StyledText {
        text: root.notif?.summary || ""
        color: Services.Colors.foreground
        font.pixelSize: root.summarySize
        font.bold: true
        width: parent.width
        wrapMode: Text.WordWrap
        visible: text !== ""
    }

    StyledText {
        id: body
        // Strip remote images before rendering the freedesktop markup subset.
        text: Services.Fmt.safeMarkup(root.notif?.body || "")
        color: Services.Colors.foreground
        font.pixelSize: root.bodySize
        width: parent.width
        wrapMode: Text.WordWrap
        maximumLineCount: root.bodyMaximumLines
        elide: Text.ElideRight
        visible: text !== ""
        textFormat: Text.StyledText

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: (body.hoveredLink !== "" || root.defaultAction)
                ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (body.hoveredLink !== "") {
                    if (Services.Fmt.openableUrl(body.hoveredLink))
                        Qt.openUrlExternally(body.hoveredLink);
                    else
                        console.warn("Notification: refusing to open", body.hoveredLink);
                } else {
                    root.invokeDefault();
                }
            }
        }
    }

    NotificationActions {
        width: parent.width
        notif: root.notif
    }
}
