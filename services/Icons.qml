pragma Singleton

import QtQuick

/// Central glyph map for the per-widget icon toggle (emoji ↔ Nerd Font).
QtObject {
    // key → { emoji, nerd }
    readonly property var map: ({
        "cpu":         { emoji: String.fromCodePoint(0x1F5A5, 0xFE0F), nerd: String.fromCodePoint(0xF0EE0) },
        "memory":      { emoji: String.fromCodePoint(0x1F9E0),         nerd: String.fromCodePoint(0xEFC5) },
        "disk":        { emoji: String.fromCodePoint(0x1F4BE),         nerd: String.fromCodePoint(0xF0A23) },
        "net":         { emoji: String.fromCodePoint(0x1F310),         nerd: String.fromCodePoint(0xF0200) },
        "power":       { emoji: String.fromCodePoint(0x23FB),          nerd: String.fromCodePoint(0xF0425) },
        "notif":       { emoji: String.fromCodePoint(0x1F514),         nerd: String.fromCodePoint(0xF009A) },
        "window":      { emoji: String.fromCodePoint(0x1FA9F),         nerd: String.fromCodePoint(0xF2D0) },
        "updates":     { emoji: String.fromCodePoint(0x1F4E6),         nerd: String.fromCodePoint(0xF06B0) },
        "uptime":      { emoji: String.fromCodePoint(0x23F1),          nerd: String.fromCodePoint(0xF0150) },
        "theme":       { emoji: String.fromCodePoint(0x1F3A8),         nerd: String.fromCodePoint(0xF03D8) },
        "layout":      { emoji: String.fromCodePoint(0x1F5C2, 0xFE0F), nerd: String.fromCodePoint(0xF0BD0) },
        "clock":       { emoji: String.fromCodePoint(0x1F550),         nerd: String.fromCodePoint(0xF0954) },
        "calendar":    { emoji: String.fromCodePoint(0x1F4C6),         nerd: String.fromCodePoint(0xF00ED) },
        "music":       { emoji: String.fromCodePoint(0x266B),          nerd: String.fromCodePoint(0xF075A) },
        "mic":         { emoji: String.fromCodePoint(0x1F399, 0xFE0F), nerd: String.fromCodePoint(0xF036C) },
        "mic-muted":   { emoji: String.fromCodePoint(0x1F636),         nerd: String.fromCodePoint(0xF036D) },
        "bright-low":  { emoji: String.fromCodePoint(0x1F505),         nerd: String.fromCodePoint(0xF00DE) },
        "bright-high": { emoji: String.fromCodePoint(0x1F506),         nerd: String.fromCodePoint(0xF00E0) },
        "vol-muted":   { emoji: String.fromCodePoint(0x1F507),         nerd: String.fromCodePoint(0xF075F) },
        "vol-low":     { emoji: String.fromCodePoint(0x1F508),         nerd: String.fromCodePoint(0xF057F) },
        "vol-med":     { emoji: String.fromCodePoint(0x1F509),         nerd: String.fromCodePoint(0xF0580) },
        "vol-high":    { emoji: String.fromCodePoint(0x1F50A),         nerd: String.fromCodePoint(0xF057E) },
        "emoji":       { emoji: String.fromCodePoint(0x1F642),         nerd: String.fromCodePoint(0xF0F5B) }
    })

    /// Glyph for a static key. set = "none" | "emoji" (default) | "nerd".
    function glyph(key, set) {
        if (set === "none") return "";
        let pair = map[key];
        if (!pair) return "";
        return set === "nerd" ? pair.nerd : pair.emoji;
    }

    /// QML color → "#rrggbb" for use inside RichText <font color>.
    function hex(c) {
        function h(x) { let s = Math.round(x * 255).toString(16); return s.length < 2 ? "0" + s : s; }
        return "#" + h(c.r) + h(c.g) + h(c.b);
    }

    /// Escape &, <, > for use as a RichText body.
    function esc(s) {
        return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    /// Wrap an already-resolved glyph in a RichText <font color> span when in
    /// nerd mode; host Text needs textFormat: Text.RichText. The three markup
    /// flavours below differ only in which getter produced the glyph, so they
    /// all route through here.
    function span(g, set, color) {
        if (set === "none" || g === "") return "";
        return set === "nerd" ? "<font color='" + hex(color) + "'>" + g + "</font>" : g;
    }

    /// span() + a trailing space (or "" for "none"), so a hidden icon leaves no
    /// leading gap in the label.
    function pad(g, set, color) {
        let s = span(g, set, color);
        return s === "" ? "" : s + " ";
    }

    function markup(key, set, color) { return span(glyph(key, set), set, color); }
    function prefix(key, set, color) { return pad(glyph(key, set), set, color); }

    /// prefix() for weather condition glyphs.
    function weatherPrefix(id, set, color) { return pad(weather(id, set), set, color); }

    /// Battery glyph by charge level + charging state (emoji or nf-md). set = "none"|"emoji"|"nerd".
    function battery(pct, charging, set) {
        if (set === "none") return "";
        if (set !== "nerd")
            return charging ? String.fromCodePoint(0x1F50C) : String.fromCodePoint(0x1F50B);
        // nf-md battery glyphs (charging, then discharging buckets rounded down to nearest 10).
        if (charging) return String.fromCodePoint(0xF0084);
        if (pct >= 95) return String.fromCodePoint(0xF0079); // full
        if (pct >= 85) return String.fromCodePoint(0xF0082); // 90
        if (pct >= 75) return String.fromCodePoint(0xF0081); // 80
        if (pct >= 65) return String.fromCodePoint(0xF0080); // 70
        if (pct >= 55) return String.fromCodePoint(0xF007F); // 60
        if (pct >= 45) return String.fromCodePoint(0xF007E); // 50
        if (pct >= 35) return String.fromCodePoint(0xF007D); // 40
        if (pct >= 25) return String.fromCodePoint(0xF007C); // 30
        if (pct >= 15) return String.fromCodePoint(0xF007B); // 20
        if (pct >= 5)  return String.fromCodePoint(0xF007A); // 10
        return String.fromCodePoint(0xF0083);                // alert
    }
    function batteryPrefix(pct, charging, set, color) {
        return pad(battery(pct, charging, set), set, color);
    }

    /// Weather glyph by OpenWeatherMap condition id (emoji or nf-weather).
    function weather(id, set) {
        if (set === "none") return "";
        if (set !== "nerd") {
            if (id >= 200 && id < 300) return String.fromCodePoint(0x26C8, 0xFE0F);
            if (id >= 300 && id < 400) return String.fromCodePoint(0x1F326, 0xFE0F);
            if (id >= 500 && id < 600) return String.fromCodePoint(0x1F327, 0xFE0F);
            if (id >= 600 && id < 700) return String.fromCodePoint(0x2744, 0xFE0F);
            if (id >= 701 && id < 800) return String.fromCodePoint(0x1F32B, 0xFE0F);
            if (id === 800)            return String.fromCodePoint(0x2600, 0xFE0F);
            if (id >= 801 && id < 900) return String.fromCodePoint(0x2601, 0xFE0F);
            return String.fromCodePoint(0x1F321, 0xFE0F);
        }
        // nf-weather
        if (id >= 200 && id < 300) return String.fromCodePoint(0xE31D); // thunderstorm
        if (id >= 300 && id < 400) return String.fromCodePoint(0xE319); // sprinkle
        if (id >= 500 && id < 600) return String.fromCodePoint(0xE318); // rain
        if (id >= 600 && id < 700) return String.fromCodePoint(0xE31A); // snow
        if (id >= 701 && id < 800) return String.fromCodePoint(0xE313); // fog
        if (id === 800)            return String.fromCodePoint(0xE30D); // clear day
        if (id >= 801 && id < 900) return String.fromCodePoint(0xE312); // cloudy
        return String.fromCodePoint(0xE350); // thermometer
    }
}
