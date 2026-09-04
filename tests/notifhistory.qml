//@ pragma UseQApplication
import QtQuick
import Quickshell
import "services" as Services

/// Regression test for notification history ordering, toast selection, and
/// lifetime policy — the logic that decides what stays in the Control Center
/// after a toast goes away.
///
/// Run with scripts/check-notif.sh, which stages this as the shell.qml of a
/// throwaway config beside copies of components/ and services/, same as
/// navcursor.qml and barreorder.qml.
///
/// It calls NotifManager's pure helpers with fake notification objects rather
/// than driving a real daemon: acquiring org.freedesktop.Notifications would
/// fight the running shell for the bus name, and the parts worth testing —
/// ordering, the maxVisible cap, expiry policy — do not need DBus at all.
ShellRoot {
    id: root

    property int failures: 0
    function check(cond, what) {
        if (!cond) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    function ids(list) { return list.map(n => n.id).join(","); }

    Component.onCompleted: {
        let N = Services.NotifManager;

        // Fake notifications: the helpers only ever look at `id`.
        let a = { id: 1 }, b = { id: 2 }, c = { id: 3 }, d = { id: 4 };
        // Deliberately NOT in arrival order, to prove the sort does the work
        // rather than the input happening to be right.
        let values = [c, a, d, b];
        let meta = {
            1: { seq: 1, expiresAt: 0, expired: false },
            2: { seq: 2, expiresAt: 0, expired: false },
            3: { seq: 3, expiresAt: 0, expired: false },
            4: { seq: 4, expiresAt: 0, expired: false }
        };

        console.warn("— history ordering");
        check(ids(N.orderNewestFirst(values, meta)) === "4,3,2,1",
            "newest first by arrival seq, not by the model's own order (got "
            + ids(N.orderNewestFirst(values, meta)) + ")");
        check(ids(N.orderNewestFirst([], meta)) === "",
            "empty model is empty, not an error");
        check(N.orderNewestFirst(undefined, meta).length === 0,
            "a null model is tolerated");
        // Order is unspecified when nothing has metadata (every comparison ties,
        // and the engine's sort stability is not ours to promise). Not losing
        // entries is the part that matters.
        check(N.orderNewestFirst(values, {}).length === 4,
            "missing metadata does not throw and drops nothing");

        console.warn("— toast selection");
        let ordered = N.orderNewestFirst(values, meta);
        check(ids(N.pickPopups(ordered, meta, 2)) === "3,4",
            "cap picks the NEWEST two, returned oldest-first so arrivals append "
            + "below (got " + ids(N.pickPopups(ordered, meta, 2)) + ")");
        check(ids(N.pickPopups(ordered, meta, 99)) === "1,2,3,4",
            "a cap above the count returns everything");
        check(N.pickPopups(ordered, meta, 0).length === 0,
            "a zero cap shows nothing");

        console.warn("— expired are excluded from toasts but not from history");
        let withExpired = {
            1: { seq: 1, expiresAt: 0, expired: true },
            2: { seq: 2, expiresAt: 0, expired: true },
            3: { seq: 3, expiresAt: 0, expired: false },
            4: { seq: 4, expiresAt: 0, expired: false }
        };
        let ord2 = N.orderNewestFirst(values, withExpired);
        check(ids(N.pickPopups(ord2, withExpired, 5)) === "3,4",
            "expired ones do not show as toasts (got "
            + ids(N.pickPopups(ord2, withExpired, 5)) + ")");
        check(ids(ord2) === "4,3,2,1",
            "...but they are all still in history — this is the actual bug that "
            + "was reported, so it gets its own assertion");
        let allGone = {
            1: { seq: 1, expiresAt: 0, expired: true },
            2: { seq: 2, expiresAt: 0, expired: true },
            3: { seq: 3, expiresAt: 0, expired: true },
            4: { seq: 4, expiresAt: 0, expired: true }
        };
        check(N.pickPopups(N.orderNewestFirst(values, allGone), allGone, 5).length === 0,
            "all expired means no toasts, so the toast window unmaps");

        console.warn("— an entry with no metadata is not toasted");
        check(ids(N.pickPopups(ordered, { 4: meta[4] }, 5)) === "4",
            "unknown ids are skipped rather than shown forever");

        console.warn("— lifetime policy");
        check(N.lifetimeOf({ urgency: 2, expireTimeout: 5000 }) === 0,
            "critical never auto-hides, whatever timeout it asked for");
        check(N.lifetimeOf({ urgency: 0, expireTimeout: 60000 }) === 5000,
            "low urgency is capped short");
        check(N.lifetimeOf({ urgency: 1, expireTimeout: 7000 }) === 7000,
            "normal urgency honours the app's timeout");
        check(N.lifetimeOf({ urgency: 1, expireTimeout: 0 })
                === Services.Settings.autoDismissSeconds * 1000,
            "timeout 0 falls back to the user's auto-dismiss setting");
        check(N.lifetimeOf({ urgency: 1, expireTimeout: -1 })
                === Services.Settings.autoDismissSeconds * 1000,
            "the spec's -1 'server decides' also falls back");
        check(N.lifetimeOf(null) === Services.Settings.autoDismissSeconds * 1000,
            "a null notification does not throw");

        console.warn("— entry animation is claimed once per notification");
        check(N.claimEntry(101) === true, "first delegate for an id animates");
        check(N.claimEntry(101) === false,
            "a rebuilt delegate for the same id does not replay it");
        check(N.claimEntry(undefined) === false, "a missing id claims nothing");

        console.warn(failures === 0
            ? "\nRESULT: all notification-history tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
