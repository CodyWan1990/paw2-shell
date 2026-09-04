import QtQuick
import QtQuick.Effects

/// Shared shadow for rounded shell surfaces.
RectangularShadow {
    property string colorValue: "#000000"
    property real colorOpacity: 0.45
    readonly property color safeColor: /^#[0-9a-fA-F]{6}$/.test(colorValue)
        ? colorValue : "#000000"

    color: Qt.rgba(safeColor.r, safeColor.g, safeColor.b, colorOpacity)
    spread: 0
    cached: true
}
