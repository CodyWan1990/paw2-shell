//@ pragma UseQApplication
import QtQuick
import Quickshell
import "services" as Services

/// Regression test for PawConfig's two rules about writing paw2's drop-in.
///
/// 1. paw2 gives a drop-in every table it names *whole*, so the first write into
///    a table has to carry the rest of that table along. Miss this and setting
///    one slider quietly resets the rest of its section to compiled defaults.
/// 2. A write paw2 refuses has to come back out of `owned`. It used to stay,
///    and since the drop-in is rewritten from `owned` every time, one refused
///    value made every later setting change a silent no-op.
///
/// Assertions run against `render(owned)` — the exact text `save()` writes —
/// rather than the file, so the test does not race FileView's IO.
///
/// Run with scripts/check-revert.sh, which stages this as the shell.qml of a
/// throwaway config and unsets PAW2_SOCKET, so the singleton cannot connect to
/// a running compositor and overwrite the config this drives by hand.
ShellRoot {
    id: root

    property int failures: 0
    function check(cond, what) {
        if (!cond) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    /// `adopt()` runs off FileView's load, so nothing can be asserted until it
    /// has reported one way or the other.
    property Timer start: Timer {
        interval: 50
        running: true
        repeat: true
        onTriggered: {
            if (!Services.PawConfig.internal.loaded)
                return;
            running = false;
            root.run();
        }
    }

    function run() {
        const C = Services.PawConfig;
        const I = C.internal;

        // paw2's effective config, as it arrives on connect.
        Services.Paw.config = {
            appearance: { gaps: 15, corner_radius: 8, blur_radius: 24, focus_ring_width: 1 },
            layout: { column_width: 0.6, center_focused_column: false }
        };

        console.warn("— claiming a table carries the whole table");
        C.set("appearance.gaps", 40);
        C.flush();
        let toml = I.render(I.owned);
        check(/gaps = 40/.test(toml), "the edit itself is written");
        check(/blur_radius = 24/.test(toml),
            "claiming [appearance] brings blur_radius with it");
        check(/focus_ring_width = 1/.test(toml), "...and every other key in it");
        check(!/column_width/.test(toml), "a table nobody wrote into is left alone");

        console.warn("— an accepted write becomes the baseline");
        Services.Paw.configResult(true, []);
        check(I.lastGood["appearance.gaps"] === 40, "paw2's yes promotes owned");

        console.warn("— a refused write does not stick");
        C.set("appearance.corner_radius", 3);
        C.flush();
        check(/corner_radius = 3/.test(I.render(I.owned)), "the write happens first");
        Services.Paw.configResult(false, ["appearance.corner_radius is nonsense"]);
        toml = I.render(I.owned);
        check(!/corner_radius = 3/.test(toml), "the refused value is taken back out");
        check(/corner_radius = 8/.test(toml), "...back to what paw2 accepted");
        check(/gaps = 40/.test(toml), "...and the edit before it survives");

        console.warn("— the next write still applies");
        C.set("appearance.blur_radius", 12);
        C.flush();
        check(/blur_radius = 12/.test(I.render(I.owned)),
            "a refusal does not turn the settings UI into a no-op");

        console.warn("— someone else's error is not ours to undo");
        Services.Paw.configResult(true, []);
        Services.Paw.configResult(false, ["config.toml: layout.column_width is bad"]);
        check(/blur_radius = 12/.test(I.render(I.owned)),
            "a hand-edit that breaks config.toml leaves the drop-in alone");

        console.warn(failures === 0
            ? "\nRESULT: all drop-in revert tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
