import QtQuick
import QtQuick.Layouts
import "../services" as Services
import "../components" as Components

/// Bar weather widget — shows temp + icon. Click opens forecast popup.
/// Replaces: waybar custom/weather module
Item {
    id: root

    property var bar: null
    property var instance: ({})
    readonly property color accent: Services.Colors.byKey(instance.color, Services.Colors.weatherUnderline)
    Layout.fillHeight: true
    implicitWidth: weatherItem.implicitWidth

    Components.BarText {
        id: weatherItem
        bar: root.bar
        small: false
        style: root.instance.style ?? "underline"
        accent: root.accent
        clickable: true
        onClicked: forecastPopup.isOpen ? forecastPopup.close() : forecastPopup.open()
        text: {
            let set = root.instance.iconSet ?? "emoji";
            if (!Services.Weather.loaded) return Services.Icons.weatherPrefix(-1, set, root.accent) + "...";
            let unit = (root.instance.showUnit ?? true) ? Services.Weather.unitSymbol : "";
            let city = (root.instance.showCity ?? false) && Services.Weather.cityName !== ""
                ? Services.Icons.esc(Services.Weather.cityName) + " " : "";
            return Services.Icons.weatherPrefix(Services.Weather.weatherId, set, root.accent)
                + city
                + "<font color='" + Services.Weather.tempColor + "'>" + Services.Weather.temp + unit + "</font>";
        }
    }

    Components.WidgetPopup {
        id: forecastPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 480
        accentColor: Services.Colors.weatherUnderline
        fadeInDuration: 500
        fadeOutDuration: 300
        useOpenDelay: true

        property string activeTab: "current"
        onOpening: activeTab = "current"

        Column {
            id: wxInnerCol
            width: parent.width
            spacing: 6

            RowLayout {
                width: parent.width
                spacing: 0

                // City and description come back from the weather API, so they
                // go through StyledText, which is PlainText by default.
                Components.StyledText {
                    text: Services.Weather.cityName
                    color: Services.Colors.foreground
                    font.pixelSize: 20
                    font.bold: true
                    Layout.fillWidth: true
                }

                Row {
                    spacing: 4

                    Rectangle {
                        width: tabCurrent.implicitWidth + 16
                        height: tabCurrent.implicitHeight + 8
                        radius: 4
                        color: forecastPopup.activeTab === "current" ? Services.Colors.weatherUnderline : Services.Colors.color0
                        Text {
                            id: tabCurrent
                            anchors.centerIn: parent
                            text: "Current"
                            color: forecastPopup.activeTab === "current" ? Services.Colors.background : Services.Colors.foreground
                            font.family: root.bar.fontFamily
                            font.pixelSize: 13
                            font.bold: forecastPopup.activeTab === "current"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: forecastPopup.activeTab = "current"
                        }
                    }

                    Rectangle {
                        width: tabForecast.implicitWidth + 16
                        height: tabForecast.implicitHeight + 8
                        radius: 4
                        color: forecastPopup.activeTab === "forecast" ? Services.Colors.weatherUnderline : Services.Colors.color0
                        Text {
                            id: tabForecast
                            anchors.centerIn: parent
                            text: "5-Day"
                            color: forecastPopup.activeTab === "forecast" ? Services.Colors.background : Services.Colors.foreground
                            font.family: root.bar.fontFamily
                            font.pixelSize: 13
                            font.bold: forecastPopup.activeTab === "forecast"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: forecastPopup.activeTab = "forecast"
                        }
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Services.Colors.color8 }

            Column {
                id: currentCol
                width: parent.width
                spacing: 12
                visible: forecastPopup.activeTab === "current"

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12

                    Text {
                        textFormat: Text.PlainText
                        text: Services.Weather.icon
                        font.family: "Noto Color Emoji"
                        font.pixelSize: 48
                        color: Services.Colors.foreground
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            textFormat: Text.RichText
                            font.family: root.bar.fontFamily
                            font.pixelSize: 36
                            font.bold: true
                            text: "<font color='" + Services.Weather.tempColor + "'>"
                                + Services.Weather.temp + Services.Weather.unitSymbol + "</font>"
                        }
                        Text {
                            textFormat: Text.RichText
                            font.family: root.bar.fontFamily
                            font.pixelSize: 14
                            text: "Feels like <font color='"
                                + Services.Weather.getTempColor(Services.Weather.feelsLike)
                                + "'>" + Services.Weather.feelsLike + Services.Weather.unitSymbol + "</font>"
                            color: Services.Colors.color8
                        }
                    }
                }

                Components.StyledText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Services.Weather.description
                    color: Services.Colors.foreground
                    font.pixelSize: 16
                    font.italic: true
                }

                Rectangle { width: parent.width; height: 1; color: Services.Colors.color8; opacity: 0.3 }

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 8

                    Text {
                        text: "\uD83D\uDCA7 Humidity"
                        color: Services.Colors.color8
                        font.family: root.bar.fontFamily
                        font.pixelSize: 15
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: Services.Weather.humidity + "%"
                        color: Services.Colors.foreground
                        font.family: root.bar.fontFamily
                        font.pixelSize: 15
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "\uD83D\uDCA8 Wind"
                        color: Services.Colors.color8
                        font.family: root.bar.fontFamily
                        font.pixelSize: 15
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: Services.Weather.windSpeed
                        color: Services.Colors.foreground
                        font.family: root.bar.fontFamily
                        font.pixelSize: 15
                        font.bold: true
                        Layout.fillWidth: true
                    }
                }
            }

            Flickable {
                id: forecastFlick
                width: parent.width
                height: Math.min(560, forecastCol.implicitHeight)
                contentHeight: forecastCol.implicitHeight
                clip: true
                visible: forecastPopup.activeTab === "forecast"
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: forecastCol
                    width: forecastFlick.width
                    spacing: 4

                    Repeater {
                        model: forecastPopup.activeTab === "forecast" ? Services.Weather.forecast : []

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: forecastCol.width
                            height: dayCol.implicitHeight + 16
                            radius: 6
                            color: modelData.isToday
                                ? Qt.tint(Services.Colors.background, Services.Colors.withAlpha(Services.Colors.weatherUnderline, 0.12))
                                : index % 2 === 0
                                    ? "transparent"
                                    : Qt.tint(Services.Colors.background, Services.Colors.withAlpha(Services.Colors.color8, 0.08))

                            Column {
                                id: dayCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                spacing: 6

                                RowLayout {
                                    width: parent.width
                                    spacing: 10

                                    Text {
                                        textFormat: Text.PlainText
                                        text: modelData.icon
                                        font.family: "Noto Color Emoji"
                                        font.pixelSize: 28
                                        color: Services.Colors.foreground
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            textFormat: Text.PlainText
                                            text: modelData.isToday
                                                ? modelData.dayName + " (Today)"
                                                : modelData.dayName
                                            color: Services.Colors.foreground
                                            font.family: root.bar.fontFamily
                                            font.pixelSize: 15
                                            font.bold: true
                                        }

                                        Text {
                                            textFormat: Text.PlainText
                                            text: modelData.description || ""
                                            color: Services.Colors.color8
                                            font.family: root.bar.fontFamily
                                            font.pixelSize: 12
                                            font.italic: true
                                            visible: text !== ""
                                        }
                                    }

                                    Text {
                                        textFormat: Text.RichText
                                        font.family: root.bar.fontFamily
                                        font.pixelSize: 18
                                        font.bold: true
                                        text: {
                                            let maxC = Services.Weather.getTempColor(modelData.tempMax);
                                            let minC = Services.Weather.getTempColor(modelData.tempMin);
                                            return "<font color='" + maxC + "'>" + modelData.tempMax + "\u00B0</font>"
                                                + "<font color='" + Services.Colors.color8 + "'> / </font>"
                                                + "<font color='" + minC + "'>" + modelData.tempMin + "\u00B0</font>";
                                        }
                                    }
                                }

                                RowLayout {
                                    width: parent.width
                                    spacing: 16

                                    Item { Layout.preferredWidth: 38 }

                                    Text {
                                        textFormat: Text.PlainText
                                        text: "\uD83D\uDCA7 " + modelData.humidity + "%"
                                        color: Services.Colors.color8
                                        font.family: root.bar.fontFamily
                                        font.pixelSize: 13
                                    }

                                    Text {
                                        textFormat: Text.PlainText
                                        text: "\uD83D\uDCA8 " + modelData.windSpeed
                                        color: Services.Colors.color8
                                        font.family: root.bar.fontFamily
                                        font.pixelSize: 13
                                    }

                                    Text {
                                        textFormat: Text.PlainText
                                        text: "\u2614 " + modelData.pop + "%"
                                        color: Services.Colors.color8
                                        font.family: root.bar.fontFamily
                                        font.pixelSize: 13
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
