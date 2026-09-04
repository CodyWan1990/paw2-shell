import QtQuick
import Quickshell
import Quickshell.Io
import "../../services" as Services
import "../../components" as Components

/// Weather settings — OpenWeatherMap API key (stored in gitignored weather.conf),
/// location + units (in settings.json via Settings.weather*).
Components.SettingsPage {
    id: root
    anchors.fill: parent

    readonly property string scriptsDir: Services.Paths.scriptsDir

    // City/Country live in settings.json (not secret). The API key is handled
    // separately below — it goes to weather.conf, never settings.json.
    readonly property var fields: [
        { label: "City", prop: "weatherCity", echo: false },
        { label: "Country", prop: "weatherCountry", echo: false }
    ]

    // Re-fetch shortly after edits so the debounced settings save has flushed.
    Timer { id: refreshDelay; interval: 800; onTriggered: Services.Weather.refresh() }

    // Load the current key from weather.conf into the field (native read; the
    // matching write still goes through weather-key.sh to preserve chmod 600).
    FileView {
        id: keyLoader
        path: Qt.resolvedUrl("file://" + Services.Paths.weatherConf)
        preload: true
        onLoaded: {
            let m = (this.text() || "").match(/^\s*OWM_API_KEY\s*=\s*"?([^"\n]*)"?\s*$/m);
            keyField.text = m ? m[1] : "";
        }
    }

    Components.SectionHeader { x: root.horizontalPadding; width: root.rowWidth; text: "Weather"; showDivider: false }

    Components.StyledText {
        x: root.horizontalPadding; width: root.rowWidth
        text: "Get a free API key at openweathermap.org. Country can be a name or 2-letter code (e.g. CA)."
        color: Services.Colors.color8
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

        // API key — stored in the gitignored weather.conf, never settings.json.
    Components.LabeledSettingRow {
        x: root.horizontalPadding; width: root.rowWidth
        label: "API key"

        Components.SettingField {
            id: keyField
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            password: true
            onEdited: v => {
                Quickshell.execDetached(["bash", root.scriptsDir + "/weather-key.sh", "set", v]);
                refreshDelay.restart();
            }
        }
    }

    Components.StyledText {
        x: root.horizontalPadding; width: root.rowWidth
        text: "The key is saved to weather.conf (gitignored) — never to settings.json."
        color: Services.Colors.color8
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }

        Repeater {
            model: root.fields

            delegate: Components.LabeledSettingRow {
                id: fieldRow
                required property var modelData
                x: root.horizontalPadding
                width: root.rowWidth
                label: fieldRow.modelData.label

                Components.SettingField {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    password: fieldRow.modelData.echo === true
                    Component.onCompleted: text = Services.Settings[fieldRow.modelData.prop]
                    onEdited: v => {
                        Services.Settings[fieldRow.modelData.prop] = v;
                        refreshDelay.restart();
                    }
                }
            }
        }

    Components.SegmentedControl {
        x: root.horizontalPadding; width: root.rowWidth
        label: "Units"
        model: [{ value: "metric", label: "Metric (°C)" }, { value: "imperial", label: "Imperial (°F)" }]
        valueRole: "value"; textRole: "label"
        currentValue: Services.Settings.weatherUnits
        onSelected: value => { Services.Settings.weatherUnits = value; refreshDelay.restart(); }
    }

    Item {
        x: root.horizontalPadding; width: root.rowWidth; height: 40

        Components.TextButton {
            text: "↻ Refresh now"
            fontSize: 14
            onClicked: Services.Weather.refresh()
        }

        Components.StyledText {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: Services.Weather.error
                ? "⚠ no data (check key/location)"
                : (Services.Weather.loaded
                    ? "✓ " + Services.Weather.cityName + "  " + Services.Weather.temp + "°"
                    : "…")
            color: Services.Weather.error ? Services.Colors.red : Services.Colors.color8
            font.pixelSize: 13
        }
    }
}
