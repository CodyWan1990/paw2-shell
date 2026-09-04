import QtQuick
import QtQuick.Layouts
import Quickshell
import "../services" as Services
import "../components" as Components

/// Three-segment clock: day, date, time.
Item {
    id: root

    property var bar: null
    property var instance: ({})
    Layout.fillHeight: true
    implicitWidth: row.implicitWidth

    // Qt format strings; "" hides the segment.
    readonly property string weekdayFormat: instance.weekdayFormat ?? "dddd"
    readonly property string dateFormat: instance.dateFormat ?? "MMMM d, yyyy"
    readonly property string timeFormat: instance.timeFormat ?? "hh:mm AP"

    // Native reactive clock. Tick per minute unless the time format asks for
    // seconds — per-minute is documented battery best-practice, and it also
    // avoids rebuilding the calendar grid every second.
    SystemClock {
        id: clock
        precision: root.timeFormat.indexOf("s") >= 0 ? SystemClock.Seconds : SystemClock.Minutes
    }
    property date now: clock.date
    // Viewed month for calendar navigation, independent of real time.
    property date viewDate: new Date()

    Components.BarItem {
        style: root.instance.style ?? "underline"
        accentColor: Services.Colors.byKey(root.instance.color, Services.Colors.clockUnderline)

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: 4

        Item {
            visible: root.weekdayFormat !== ""
            Layout.fillHeight: true
            Layout.preferredWidth: visible ? dayText.implicitWidth + 10 : 0
            Layout.topMargin: 1
            Layout.bottomMargin: 1

            Text {
                id: dayText
                anchors.centerIn: parent
                textFormat: Text.RichText
                text: Services.Icons.prefix("calendar", root.instance.iconSet ?? "emoji", Services.Colors.byKey(root.instance.color, Services.Colors.clockUnderline)) + root.now.toLocaleDateString(Qt.locale(), root.weekdayFormat)
                color: Services.Colors.foreground
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.fontSize
                font.weight: Font.Bold
            }

        }

        Item {
            visible: root.dateFormat !== ""
            Layout.fillHeight: true
            Layout.preferredWidth: visible ? dateText.implicitWidth + 10 : 0
            Layout.topMargin: 1
            Layout.bottomMargin: 1

            Text {
                textFormat: Text.PlainText
                id: dateText
                anchors.centerIn: parent
                text: root.now.toLocaleDateString(Qt.locale(), root.dateFormat)
                color: Services.Colors.foreground
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.fontSize
                font.weight: Font.Bold
            }


            MouseArea {
                anchors.fill: parent
                onClicked: calendarPopup.isOpen ? calendarPopup.close() : calendarPopup.open()
            }
        }

        Item {
            visible: root.timeFormat !== ""
            Layout.fillHeight: true
            Layout.preferredWidth: visible ? timeText.implicitWidth + 10 : 0
            Layout.topMargin: 1
            Layout.bottomMargin: 1

            Text {
                id: timeText
                anchors.centerIn: parent
                textFormat: Text.RichText
                text: Services.Icons.prefix("clock", root.instance.iconSet ?? "emoji", Services.Colors.byKey(root.instance.color, Services.Colors.clockUnderline)) + root.now.toLocaleTimeString(Qt.locale(), root.timeFormat)
                color: Services.Colors.foreground
                font.family: root.bar.fontFamily
                font.pixelSize: root.bar.fontSize
                font.weight: Font.Bold
            }

        }
    }
    }

    Components.WidgetPopup {
        id: calendarPopup
        bar: root.bar
        parentWidget: root
        contentWidth: 330
        contentHeight: 350
        contentMargins: 16
        accentColor: Services.Colors.clockUnderline
        cornerRadius: 0
        fadeInDuration: 500
        fadeOutDuration: 300
        useOpenDelay: true
        xClamp: 0
        onOpening: root.viewDate = new Date(root.now)

        Column {
            anchors.fill: parent
            spacing: 8

            Item {
                width: parent.width
                height: 28

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u2039"
                    color: prevMA.containsMouse
                        ? Services.Colors.foreground : Services.Colors.color8
                    font.family: root.bar.fontFamily
                    font.pixelSize: 24
                    font.bold: true

                    MouseArea {
                        id: prevMA
                        anchors.fill: parent
                        anchors.margins: -8
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.prevMonth()
                    }
                }

                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: root.viewDate.toLocaleDateString(Qt.locale(), "MMMM yyyy")
                    color: Services.Colors.foreground
                    font.family: root.bar.fontFamily
                    font.pixelSize: 17
                    font.bold: true

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.goToToday()
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u203A"
                    color: nextMA.containsMouse
                        ? Services.Colors.foreground : Services.Colors.color8
                    font.family: root.bar.fontFamily
                    font.pixelSize: 24
                    font.bold: true

                    MouseArea {
                        id: nextMA
                        anchors.fill: parent
                        anchors.margins: -8
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.nextMonth()
                    }
                }
            }

            Components.Separator { opacity: 0.25 }

            Row {
                spacing: 0
                anchors.horizontalCenter: parent.horizontalCenter
                Repeater {
                    model: ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
                    Text {
                        textFormat: Text.PlainText
                        required property string modelData
                        required property int index
                        width: 42
                        text: modelData
                        color: (index === 0 || index === 6)
                            ? Services.Colors.clockUnderline
                            : Services.Colors.color8
                        font.family: root.bar.fontFamily
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Grid {
                columns: 7
                spacing: 0
                anchors.horizontalCenter: parent.horizontalCenter

                Repeater {
                    id: dayRepeater
                    model: root.calendarDays()
                    delegate: Item {
                        required property var modelData
                        width: 42
                        height: 38

                        Rectangle {
                            anchors.centerIn: parent
                            width: 32; height: 32; radius: 16
                            color: Services.Colors.foreground
                            opacity: !modelData.isToday
                                && dayHover.containsMouse
                                && modelData.isCurrentMonth ? 0.08 : 0
                            Behavior on opacity {
                                NumberAnimation { duration: 150 }
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 32; height: 32; radius: 16
                            color: modelData.isToday
                                ? Services.Colors.clockUnderline : "transparent"
                        }

                        Text {
                            textFormat: Text.PlainText
                            anchors.centerIn: parent
                            text: modelData.day.toString()
                            color: {
                                if (modelData.isToday)
                                    return Services.Colors.background;
                                if (modelData.isCurrentMonth)
                                    return Services.Colors.foreground;
                                return Services.Colors.color8;
                            }
                            opacity: modelData.isCurrentMonth ? 1.0 : 0.35
                            font.family: root.bar.fontFamily
                            font.pixelSize: 14
                            font.bold: modelData.isToday
                            horizontalAlignment: Text.AlignHCenter
                        }

                        MouseArea {
                            id: dayHover
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }
        }
    }

    function prevMonth() {
        viewDate = new Date(viewDate.getFullYear(), viewDate.getMonth() - 1, 1);
    }
    function nextMonth() {
        viewDate = new Date(viewDate.getFullYear(), viewDate.getMonth() + 1, 1);
    }
    function goToToday() {
        viewDate = new Date(now);
    }

    /// Build a 6×7 grid of day objects for the viewed month.
    function calendarDays(): list<var> {
        let days = [];
        let year = viewDate.getFullYear();
        let month = viewDate.getMonth();
        let todayDate = now.getDate();
        let todayMonth = now.getMonth();
        let todayYear = now.getFullYear();

        let firstDay = new Date(year, month, 1).getDay(); // 0=Sun
        let daysInMonth = new Date(year, month + 1, 0).getDate();
        let daysInPrevMonth = new Date(year, month, 0).getDate();

        for (let i = firstDay - 1; i >= 0; i--)
            days.push({ day: daysInPrevMonth - i, isToday: false, isCurrentMonth: false });

        for (let d = 1; d <= daysInMonth; d++)
            days.push({
                day: d,
                isToday: d === todayDate && month === todayMonth && year === todayYear,
                isCurrentMonth: true
            });

        // Always fill to 42 (6 rows).
        let nextDay = 1;
        while (days.length < 42)
            days.push({ day: nextDay++, isToday: false, isCurrentMonth: false });

        return days;
    }
}
