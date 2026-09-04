pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Services.Notifications
import "." as Services

/// Notification manager singleton — acts as the org.freedesktop.Notifications daemon.
/// Replaces swaync. WARNING: swaync must not be running simultaneously.
///
/// Owns expiry policy as well as storage. A toast timing out must not call
/// dismiss(): that closes the notification and Quickshell destroys the object,
/// which is how notifications used to disappear from the Control Center before
/// anyone had read them. Expiry here only hides the *toast*; the notification
/// lives in trackedNotifications until the user clears it or it falls off the
/// history limit — so its actions stay invokable from the Control Center.
QtObject {
    id: root

    readonly property var notifications: server.trackedNotifications
    property bool dndEnabled: false

    /// id → { seq, expiresAt, expired }. Always reassigned, never mutated in
    /// place, or the derived properties below would not re-evaluate.
    property var meta: ({})
    property int seqCounter: 0

    /// Everything tracked, newest first, for the Control Center. Ordered by our
    /// own arrival counter rather than by trackedNotifications' order, which
    /// Quickshell does not document and which nothing should depend on.
    readonly property var history: orderNewestFirst(server.trackedNotifications.values, root.meta)

    /// Pure, so the ordering is testable without a DBus daemon behind it.
    function orderNewestFirst(values, m) {
        let out = (values ?? []).slice();
        out.sort((a, b) => (m[b.id]?.seq ?? 0) - (m[a.id]?.seq ?? 0));
        return out;
    }

    /// What is on screen as a toast: the newest maxVisible unexpired ones,
    /// oldest first so an arrival appends below and the ones already up do not
    /// jump. Rebuilt on every change, which recreates the toast delegates —
    /// harmless now that they own no timer, and claimEntry() keeps the entry
    /// animation from replaying.
    readonly property var popups: pickPopups(root.history, root.meta, Services.Settings.maxVisible)

    /// Pure, for the same reason as orderNewestFirst(). `ordered` is
    /// newest-first; the result is oldest-first.
    function pickPopups(ordered, m, cap) {
        let out = [];
        for (let n of ordered) {
            if (out.length >= cap)     // before the push, or cap 0 shows one
                break;
            let e = m[n.id];
            if (!e || e.expired)
                continue;
            out.push(n);
        }
        return out.reverse();
    }

    /// Ids whose toast has already played its entry animation. Deliberately
    /// mutated in place and read by no binding: reassigning it would invalidate
    /// popups from inside a delegate's Component.onCompleted, which is a loop.
    readonly property var animatedIds: ({})
    function claimEntry(id) {
        if (id === undefined || root.animatedIds[id])
            return false;
        root.animatedIds[id] = true;
        return true;
    }

    property NotificationServer server: NotificationServer {
        id: server
        bodySupported: true
        bodyMarkupSupported: true
        bodyHyperlinksSupported: true
        actionsSupported: true
        persistenceSupported: true
        keepOnReload: true

        onNotification: notification => {
            notification.tracked = true;
            root.register(notification);
        }
    }

    /// Marks toasts whose deadline has passed. One timer for all of them: a
    /// timer per delegate dies and restarts with its delegate, so a burst of
    /// new notifications used to be able to extend the ones already on screen.
    property Timer sweeper: Timer {
        interval: 250
        repeat: true
        running: false
        onTriggered: root.sweep()
    }

    /// How long this notification stays on screen, in ms. 0 means "until the
    /// user acts".
    function lifetimeOf(n) {
        if (!n)
            return Services.Settings.autoDismissSeconds * 1000;
        if (n.urgency === 2)
            return 0;                              // critical: never auto-hide
        if (n.urgency === 0)
            return 5000;                           // low: shorter
        if (n.expireTimeout > 0)
            return n.expireTimeout;
        return Services.Settings.autoDismissSeconds * 1000;
    }

    function register(n) {
        let life = lifetimeOf(n);
        let m = Object.assign({}, root.meta);
        m[n.id] = {
            seq: ++root.seqCounter,
            expiresAt: life > 0 ? Date.now() + life : 0,
            expired: false
        };
        root.meta = m;
        root.trim();
        root.sweeper.running = true;
    }

    /// Drop the oldest once the history outgrows its limit, so a long session
    /// does not accumulate live notification objects and their images forever.
    function trim() {
        let limit = Services.Settings.historyLimit;
        let all = root.history;            // newest first
        for (let i = limit; i < all.length; i++)
            all[i].dismiss();
    }

    function sweep() {
        let now = Date.now();
        let m = Object.assign({}, root.meta);
        let changed = false;
        let pending = false;
        for (let n of (server.trackedNotifications.values ?? [])) {
            let e = m[n.id];
            if (!e || e.expired || e.expiresAt === 0)
                continue;
            if (now >= e.expiresAt) {
                m[n.id] = { seq: e.seq, expiresAt: e.expiresAt, expired: true };
                changed = true;
            } else {
                pending = true;
            }
        }
        if (changed)
            root.meta = m;
        /* Nothing left counting down — stop ticking until the next arrival. */
        if (!pending)
            root.sweeper.running = false;
    }

    /// Forget metadata for notifications that are gone, so the map does not
    /// grow for the life of the session.
    function prune() {
        let live = ({});
        for (let n of (server.trackedNotifications.values ?? []))
            live[n.id] = true;
        let m = ({});
        let dropped = false;
        for (let id in root.meta) {
            if (live[id])
                m[id] = root.meta[id];
            else
                dropped = true;
        }
        for (let id in root.animatedIds) {
            if (!live[id])
                delete root.animatedIds[id];
        }
        if (dropped)
            root.meta = m;
    }

    function dismissAll() {
        // Dismiss from end to avoid index shifting
        let count = server.trackedNotifications.values?.length ?? 0;
        for (let i = count - 1; i >= 0; i--) {
            try {
                let n = server.trackedNotifications.values[i];
                if (n) n.dismiss();
            } catch (e) {
                console.warn("NotifManager: dismiss error:", e);
            }
        }
        root.meta = ({});
    }

    property Connections modelWatch: Connections {
        target: server.trackedNotifications
        function onValuesChanged() { root.prune(); }
    }

    Component.onCompleted: {
        console.log("NotifManager: notification daemon started");
    }
}
