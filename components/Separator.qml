import QtQuick
import "../services" as Services

/// 1px horizontal divider line. Used across popups for visual separation.
Rectangle {
    width: parent ? parent.width : 0
    height: 1
    color: Services.Colors.color8
    opacity: 0.3
}
