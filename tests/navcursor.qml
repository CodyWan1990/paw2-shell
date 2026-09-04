//@ pragma UseQApplication
import QtQuick
import Quickshell
import "components" as C

/// Regression test for the settings row cursor (KeyNavCursor + SettingsPage).
///
/// Run with scripts/check-nav.sh, which stages this as the shell.qml of a
/// throwaway config next to copies of components/ and services/ — Quickshell
/// refuses to import a module from outside its config folder, so the test
/// cannot simply live beside them and import upwards.
///
/// It runs inside Quickshell so the singletons the controls import actually
/// resolve, and drives the cursor through the same public API AppLauncher
/// uses. It asserts on what the controls end up holding rather than on key
/// events: paw overrides a virtual keyboard's keymap (inherited from dwl), so
/// wtype and friends cannot drive a nested session, and a test that pretended
/// otherwise would be testing the harness.
ShellRoot {
    id: root

    property int failures: 0
    function check(cond, what) {
        if (!cond) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    C.SettingsPage {
        id: page
        width: 600
        height: 400

        C.SectionHeader { width: 560; text: "Header is not a stop" }

        C.StyledSlider {
            id: slider
            width: 560
            label: "Slider"; from: 0; to: 100; stepSize: 5
            property real held: 40
            value: held
            onMoved: v => held = v
        }

        C.SegmentedControl {
            id: seg
            width: 560
            label: "Segmented"
            model: ["a", "b", "c"]
            property string held: "a"
            currentValue: held
            onSelected: v => held = v
        }

        C.StyledDropdown {
            id: drop
            width: 560
            label: "Dropdown"
            model: ["one", "two", "three"]
            property string held: "one"
            currentValue: held
            onSelected: v => held = v
        }

        C.LabeledSettingRow {
            width: 560
            label: "Wrapped field"
            C.SettingField { id: field; width: 300 }
        }

        C.TextButton {
            id: button
            text: "Press me"
            property int presses: 0
            onClicked: presses++
        }

        C.StyledText { width: 560; text: "Plain text is not a stop either" }
    }

    // The Compositor tab is ~60 of these, so the forwarding matters more than
    // any single control.
    C.SettingsPage {
        id: specPage
        width: 600
        height: 400

        C.SettingsOptionEditor {
            id: sectionRow
            width: 560
            spec: ({ section: "A heading" })
        }
        C.SettingsOptionEditor {
            id: noteRow
            width: 560
            spec: ({ type: "note", text: "explanatory text" })
        }
        C.SettingsOptionEditor {
            id: boolRow
            width: 560
            spec: ({ type: "bool", label: "A toggle" })
            property bool held: false
            value: held
            onValueEdited: v => held = v
        }
        C.SettingsOptionEditor {
            id: intRow
            width: 560
            spec: ({ type: "int", label: "A number", from: 0, to: 50, step: 5 })
            property int held: 10
            value: held
            onValueEdited: v => held = v
        }
    }

    Component.onCompleted: {
        page.refreshStops();

        console.warn("— stop collection");
        check(page.stopCount === 5,
            "collects 5 stops, skipping header and plain text (got " + page.stopCount + ")");

        console.warn("— cursor movement and painting");
        page.stepCursor(1);
        check(page.cursorIndex === 0, "first Down lands on stop 0");
        check(slider.cursor === true, "slider shows the cursor");
        page.stepCursor(1);
        check(page.cursorIndex === 1 && seg.cursor === true && slider.cursor === false,
            "cursor moves on and the previous stop is cleared");

        console.warn("— Left/Right adjust in place");
        page.adjust(1);
        check(seg.held === "b", "segmented steps forward (got " + seg.held + ")");
        page.adjust(-1);
        check(seg.held === "a", "segmented steps back");
        page.stepCursor(-1);
        page.adjust(1);
        check(slider.held === 45, "slider adds one stepSize (got " + slider.held + ")");
        page.adjust(-100);
        check(slider.held === 0, "slider clamps at `from` (got " + slider.held + ")");

        console.warn("— Enter activates");
        page.stepCursor(1); page.stepCursor(1);   // -> dropdown
        check(page.cursorIndex === 2, "cursor on the dropdown");
        page.activate();
        check(drop.expanded === true, "Enter opens the dropdown");
        drop.collapse();
        check(drop.expanded === false, "collapse closes it");

        console.warn("— Left/Right on a closed dropdown steps the value");
        page.adjust(1);
        check(drop.held === "two", "dropdown steps without opening (got " + drop.held + ")");

        console.warn("— wrapped control is reached through its row");
        page.stepCursor(1);
        check(page.cursorIndex === 3, "cursor on the labelled row");
        check(field.cursor === true,
            "row forwards the cursor highlight to the field it wraps");
        page.stepCursor(1);
        check(field.cursor === false, "and clears it on the way out");
        page.stepCursor(-1);

        console.warn("— button");
        page.stepCursor(1);
        page.activate();
        check(button.presses === 1, "Enter presses the button (got " + button.presses + ")");

        console.warn("— clamping");
        page.stepCursor(1);
        check(page.cursorIndex === 4, "cannot move past the last stop");
        page.clearCursor();
        check(page.cursorIndex === -1 && button.cursor === false,
            "clearCursor drops the highlight");
        check(page.stepCursor(-1) === true, "step from cleared state re-enters");
        page.setCursor(0);
        check(page.stepCursor(-1) === false,
            "stepping up off the top reports false, so the host can park on the tabs");

        console.warn("— spec-driven rows (the Compositor tab's mechanism)");
        specPage.refreshStops();
        check(specPage.stopCount === 2,
            "a section heading and a note contribute no stops (got "
            + specPage.stopCount + ")");
        check(sectionRow.selectableCount === 0 && noteRow.selectableCount === 0,
            "...because they report none themselves");
        specPage.stepCursor(1);
        check(boolRow.cursorIndex === 0, "cursor reaches the bool row's editor");
        specPage.adjust(1);
        check(boolRow.held === true, "Right toggles it through the loaded editor");
        specPage.stepCursor(1);
        check(boolRow.cursorIndex === -1 && intRow.cursorIndex === 0,
            "cursor moves to the slider row and the bool row is cleared");
        specPage.adjust(1);
        check(intRow.held === 15, "Right adds one step (got " + intRow.held + ")");

        console.warn(failures === 0
            ? "\nRESULT: all row-cursor tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
