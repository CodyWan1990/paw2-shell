import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Package list and action shared by update providers.
Column {
    id: root

    property string icon: ""
    property string title: ""
    property int count: 0
    property bool checking: false
    property var packages: []
    property var detailText: function(packageData) { return ""; }
    property int detailWidth: 220
    property string actionText: ""
    property color actionColor: Services.Colors.accent
    signal actionRequested()

    width: parent ? parent.width : 0
    spacing: 6

    RowLayout {
        width: parent.width
        StyledText {
            text: root.icon + " " + root.title
            color: Services.Colors.foreground
            font.pixelSize: 15
            font.bold: true
            Layout.fillWidth: true
        }
        StyledText {
            text: root.checking ? "↻" : root.count > 0 ? root.count + "" : "✅"
            color: root.checking || root.count > 0
                ? Services.Colors.foreground : Services.Colors.green
            font.pixelSize: 13
        }
    }

    ListView {
        width: parent.width
        height: Math.min(200, contentHeight)
        clip: true
        visible: root.count > 0
        model: root.packages

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: ListView.view.width
            height: packageRow.implicitHeight + 6
            color: index % 2 === 0
                ? "transparent" : Services.Colors.withAlpha(Services.Colors.color8, 0.08)

            RowLayout {
                id: packageRow
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4

                StyledText {
                    text: modelData.name || ""
                    color: Services.Colors.foreground
                    font.pixelSize: 13
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                StyledText {
                    text: root.detailText(modelData)
                    color: Services.Colors.color8
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.detailWidth
                }
            }
        }
    }

    TextButton {
        visible: !root.checking && root.count > 0
        text: root.actionText
        fontBold: true
        idleColor: root.actionColor
        hoverColor: Services.Colors.withAlpha(root.actionColor, 0.8)
        idleBorderColor: root.actionColor
        hoverBorderColor: root.actionColor
        idleTextColor: Services.Colors.background
        hoverTextColor: Services.Colors.background
        onClicked: root.actionRequested()
    }
}
