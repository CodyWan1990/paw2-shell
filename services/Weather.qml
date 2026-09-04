pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import "." as Services

/// OpenWeatherMap weather provider — fetches directly via XMLHttpRequest every 15min.
/// City/country/units come from Settings; the API key is read from weather.conf
/// (gitignored, written by weather-key.sh which keeps it chmod 600).
QtObject {
    readonly property string cityName: internal.cityName
    readonly property int temp: internal.temp
    readonly property int feelsLike: internal.feelsLike
    readonly property string description: internal.description
    readonly property int humidity: internal.humidity
    readonly property string windSpeed: internal.windSpeed
    readonly property string icon: internal.icon
    readonly property int weatherId: internal.weatherId
    readonly property color tempColor: getTempColor(temp)
    /// Degree symbol matching Settings.weatherUnits — the API is asked for the
    /// same units, so labelling every reading °C was simply wrong on imperial.
    readonly property string unitSymbol:
        Services.Settings.weatherUnits === "imperial" ? "°F" : "°C"

    readonly property var forecast: internal.forecast

    readonly property bool loaded: internal.loaded
    readonly property bool error: internal.hasError

    /// Force a re-fetch (call after editing weather settings).
    function refresh() { internal.requestFetch(); }

    function getTempColor(t) {
        if (t < -20) return "#4169E1";
        if (t < 0)   return "#1E90FF";
        if (t < 15)  return "#FFFFFF";
        if (t < 25)  return "#FFFF00";
        if (t < 35)  return "#FFA500";
        return "#FF0000";
    }


    readonly property QtObject internal: QtObject {
        id: priv
        property string cityName: ""
        property int temp: 0
        property int feelsLike: 0
        property string description: ""
        property int humidity: 0
        property string windSpeed: "0.0"
        property string icon: ""
        property int weatherId: 0
        property var forecast: []
        property bool loaded: false
        property bool hasError: false

        property string lat: ""
        property string lon: ""

        property string confPath: Services.Paths.weatherConf

        // weather.conf holds the API key (and optional OWM_* fallbacks). Read-only here;
        // weather-key.sh remains the writer (preserves chmod 600 on the secret).
        // FileView loads async — fetch only once `loaded` fires, else text() is empty.
        property FileView confView: FileView {
            path: Qt.resolvedUrl("file://" + priv.confPath)
            watchChanges: true
            preload: true
            onLoaded: priv.requestFetch()
            onFileChanged: this.reload()   // reload re-emits loaded → re-fetch on key edits
        }

        // Coalesce the startup triggers (key file load + city/country/units arriving)
        // into a single fetch. ponytail: 250ms debounce so N triggers ≠ N API calls.
        property Timer fetchDebounce: Timer { interval: 250; onTriggered: priv.fetchCurrent() }
        function requestFetch() { fetchDebounce.restart(); }

        // Settings is a separate singleton that loads independently — re-fetch when its
        // weather inputs become available or change.
        property Connections settingsConn: Connections {
            target: Services.Settings
            function onWeatherCityChanged()    { priv.requestFetch(); }
            function onWeatherCountryChanged() { priv.requestFetch(); }
            function onWeatherUnitsChanged()   { priv.requestFetch(); }
        }

        // Parse `OWM_X="Y"` lines into { X: "Y" }.
        function conf() {
            let out = {};
            let txt = confView.text() || "";
            let re = /^\s*OWM_([A-Za-z_]+)\s*=\s*"?([^"\n]*)"?\s*$/gm;
            let m;
            while ((m = re.exec(txt)) !== null) out[m[1]] = m[2];
            return out;
        }

        function titleCase(s) {
            return (s || "").replace(/\w\S*/g, w => w.charAt(0).toUpperCase() + w.slice(1));
        }

        function fetchCurrent() {
            let c = conf();
            let key = c.API_KEY || "";
            let city = Services.Settings.weatherCity || c.CITY || "";
            let country = Services.Settings.weatherCountry || c.COUNTRY || "";
            let units = Services.Settings.weatherUnits || c.UNITS || "metric";
            if (!key || !city) { priv.hasError = true; return; }

            let q = country ? city + "," + country : city;
            let url = "https://api.openweathermap.org/data/2.5/weather?q="
                + encodeURIComponent(q) + "&units=" + units + "&appid=" + key;

            let xhr = new XMLHttpRequest();
            xhr.timeout = 15000;  // don't let a hung request hold the closure forever
            xhr.ontimeout = function () { console.warn("Weather: current request timed out"); priv.hasError = true; };
            xhr.onreadystatechange = function () {
                if (xhr.readyState !== XMLHttpRequest.DONE) return;
                if (xhr.status !== 200) {
                    console.warn("Weather: current HTTP", xhr.status);
                    priv.hasError = true;
                    return;
                }
                try {
                    let d = JSON.parse(xhr.responseText);
                    if (String(d.cod) !== "200") { priv.hasError = true; return; }
                    priv.cityName = d.name || "";
                    priv.temp = Math.round(d.main.temp);
                    priv.feelsLike = Math.round(d.main.feels_like);
                    priv.description = priv.titleCase(d.weather[0].description);
                    priv.humidity = d.main.humidity;
                    priv.windSpeed = (d.wind && d.wind.speed ? d.wind.speed : 0).toFixed(1);
                    priv.weatherId = d.weather[0].id;
                    priv.icon = Services.Icons.weather(priv.weatherId, "emoji");
                    priv.loaded = true;
                    priv.hasError = false;
                    priv.lat = String(d.coord.lat);
                    priv.lon = String(d.coord.lon);
                    console.log("Weather: current loaded, temp=" + priv.temp + ", fetching forecast...");
                    priv.fetchForecast();
                } catch (e) {
                    console.warn("Weather: current parse error:", e, "raw:", xhr.responseText);
                    priv.hasError = true;
                }
            };
            xhr.open("GET", url);
            xhr.send();
        }

        function fetchForecast() {
            if (!lat || !lon) return;
            let c = conf();
            let key = c.API_KEY || "";
            let units = Services.Settings.weatherUnits || c.UNITS || "metric";
            if (!key) return;

            let url = "https://api.openweathermap.org/data/3.0/onecall?lat=" + lat + "&lon=" + lon
                + "&exclude=current,minutely,hourly,alerts&units=" + units + "&appid=" + key;

            let xhr = new XMLHttpRequest();
            xhr.timeout = 15000;
            xhr.ontimeout = function () { console.warn("Weather: forecast request timed out"); };
            xhr.onreadystatechange = function () {
                if (xhr.readyState !== XMLHttpRequest.DONE) return;
                if (xhr.status !== 200) { console.warn("Weather: forecast HTTP", xhr.status); return; }
                try {
                    let d = JSON.parse(xhr.responseText);
                    let days = (d.daily || []).slice(0, 8);
                    if (days.length === 0) return;

                    let result = [];
                    for (let i = 0; i < days.length; i++) {
                        let day = days[i];
                        let date = new Date(day.dt * 1000);
                        result.push({
                            dayName: date.toLocaleDateString(Qt.locale(), "dddd, MMM d"),
                            tempMin: Math.round(day.temp.min),
                            tempMax: Math.round(day.temp.max),
                            description: priv.titleCase(day.weather[0].description),
                            icon: Services.Icons.weather(day.weather[0].id, "emoji"),
                            humidity: day.humidity || 0,
                            windSpeed: (day.wind_speed || 0).toFixed(1),
                            pop: Math.round((day.pop || 0) * 100),
                            isToday: i === 0
                        });
                    }
                    priv.forecast = result;
                    console.log("Weather: forecast loaded,", result.length, "days");
                } catch (e) {
                    console.warn("Weather: forecast parse error:", e, "raw:", xhr.responseText);
                }
            };
            xhr.open("GET", url);
            xhr.send();
        }

        property Timer pollTimer: Timer {
            interval: 900000
            running: true
            repeat: true
            onTriggered: priv.requestFetch()
        }

        Component.onCompleted: {
            console.log("Weather: starting (XHR, key from weather.conf)");
            requestFetch();
        }
    }
}
