pragma Singleton

import QtQuick

/// Tiny template helper for widget display formats.
/// Widgets expose a `format` option like "{used}/{total}GB ({pct}%)" via
/// Settings.widgetSpecs; this expands it. Values may contain RichText markup
/// (the colored <font> spans the monitors use), so nothing is escaped here —
/// callers escape untrusted parts before handing them over.
QtObject {
    /// Replace every {name} in tmpl with vals[name]. Unknown names expand to "".
    function fill(tmpl, vals) {
        return String(tmpl).replace(/\{(\w+)\}/g, (m, k) => vals[k] !== undefined ? String(vals[k]) : "");
    }

    /// Strip <img> from a notification body before it reaches a StyledText.
    ///
    /// The freedesktop spec's markup subset is <b> <i> <u> <a> <img>, and any
    /// session-bus peer can send it. Qt honours <img src="..."> by fetching the
    /// URL, so an unread notification is enough to leak a read-receipt and the
    /// machine's IP, or to pull a local file into the rendered surface. The
    /// other four tags are inert; only this one reaches out.
    function safeMarkup(s) {
        return String(s ?? "").replace(/<\s*img\b[^>]*>/gi, "");
    }

    /// Whether a notification link is safe to hand to xdg-open on a click.
    /// The href is attacker-chosen, and Qt.openUrlExternally will happily launch
    /// a file:// path or a registered custom scheme.
    function openableUrl(url) {
        return /^(https?|mailto):/i.test(String(url ?? ""));
    }
}
