import QtQuick
import QtQuick.Layouts

/// Repeated dynamic-widget loader used by each TopBar section.
RowLayout {
    id: root

    property var model: []
    property var hostBar: null
    property var registry: ({})
    property var resolveComponent: function(type) { return null; }

    Repeater {
        model: root.model

        delegate: Loader {
            required property var modelData
            Layout.fillHeight: true
            Layout.preferredWidth: item ? item.implicitWidth : 0
            sourceComponent: root.resolveComponent(modelData.type)
            source: !sourceComponent ? (root.registry[modelData.type] ?? "") : ""
            onLoaded: {
                if (item && "bar" in item) item.bar = root.hostBar;
                if (item && "instance" in item) item.instance = modelData;
            }
        }
    }
}
