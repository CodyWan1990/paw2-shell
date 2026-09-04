import QtQuick
import QtQuick.Layouts
import Quickshell
import "../services" as Services

/// Primary screen, resolution, refresh rate, scale and orientation for every output.
///
/// Shared by the first-run Welcome flow and Settings → Monitors, so changing a
/// display afterwards doesn't mean re-running setup. Owns no state: settings
/// go through PawConfig and live display changes go through Outputs.
///
/// Hosts supply their own scrolling — this is a plain Item sized by its content.
/// `cursorIndex` is for keyboard-driven hosts; leave it at -1 for mouse-only
/// ones, and drive it together with `selectableCount` and `activateAt()`.
Item {
    id: root

    /// Which selectable carries the keyboard cursor; -1 for none.
    property int cursorIndex: -1

    readonly property int controlsPerOutput: 5
    readonly property var transforms: [
        { label: "Normal", value: "normal" },
        { label: "90°", value: "90" },
        { label: "180°", value: "180" },
        { label: "270°", value: "270" },
        { label: "Flipped", value: "flipped" },
        { label: "Flipped 90°", value: "flipped-90" },
        { label: "Flipped 180°", value: "flipped-180" },
        { label: "Flipped 270°", value: "flipped-270" }
    ]
    readonly property var transformLabels: transforms.map(item => item.label)

    function transformLabel(value) {
        const item = transforms.find(candidate => candidate.value === value);
        return item ? item.label : "Normal";
    }

    function transformValue(label) {
        const item = transforms.find(candidate => candidate.label === label);
        return item ? item.value : "normal";
    }

    /// Output names for the primary picker. Prefers wlr-randr (which also
    /// knows about outputs that are currently off), falls back to the screens
    /// Quickshell itself sees, so this works with wlr-randr absent.
    readonly property var screenNames: {
        if (Services.Outputs.available && Services.Outputs.list.length > 0)
            return Services.Outputs.list.map(o => o.name);
        let out = [];
        for (let i = 0; i < Quickshell.screens.length; i++)
            out.push(Quickshell.screens[i].name);
        return out;
    }

    /// Cursor positions: one per screen chip, then every output control.
    readonly property int selectableCount: screenNames.length
        + (Services.Outputs.available ? Services.Outputs.list.length : 0)
            * controlsPerOutput

    /// One output's `[[monitors]]` entry in paw2's config, or null when it has
    /// none — in which case every setting is at paw2's default.
    function configuredMonitor(name) {
        return (Services.PawConfig.get("monitors", []) ?? [])
            .find(m => m.connector === name) ?? null;
    }

    implicitHeight: content.implicitHeight

    /// Act on the selectable at `i` — toggle a screen chip, or open the
    /// dropdown. Keeps the row delegates private to this file.
    function activateAt(i) {
        if (i < 0 || i >= selectableCount)
            return;
        if (i < screenNames.length) {
            const name = screenNames[i];
            Services.PawConfig.set("appearance.startupoutput",
                Services.PawConfig.get("appearance.startupoutput", "") === name
                    ? "" : name);
            return;
        }
        // Repeater delegates aren't addressable by id, so the row exposes its
        // pieces and we ask the right one to act.
        const rest = i - screenNames.length;
        const row = outputRows.itemAt(Math.floor(rest / controlsPerOutput));
        if (!row)
            return;
        const control = rest % controlsPerOutput;
        if (control === 0)
            row.resDrop.expand();
        else if (control === 1)
            row.rateDrop.expand();
        else if (control === 2)
            row.scaleSlider.keyActivate();
        else if (control === 3)
            row.transformDrop.expand();
        else
            row.cycleVrr();
    }

    /// Left/Right on a selectable: step a mode or the VRR policy in place,
    /// without opening anything. A screen chip has nothing to step.
    function adjustAt(i, delta) {
        if (i < screenNames.length || i >= selectableCount)
            return;
        const rest = i - screenNames.length;
        const row = outputRows.itemAt(Math.floor(rest / controlsPerOutput));
        if (!row)
            return;
        const control = rest % controlsPerOutput;
        if (control === 0)
            row.resDrop.keyAdjust(delta);
        else if (control === 1)
            row.rateDrop.keyAdjust(delta);
        else if (control === 2)
            row.scaleSlider.keyAdjust(delta);
        else if (control === 3)
            row.transformDrop.keyAdjust(delta);
        else
            row.cycleVrr();
    }

    // The settings tab instantiates this through a Loader, so it has to ask for
    // outputs itself; Welcome refreshes again each time it reopens.
    Component.onCompleted: Services.Outputs.refresh()

    ColumnLayout {
        id: content
        width: root.width
        spacing: 12

        StyledText {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "The primary screen is where paw puts the focus when it "
                + "starts. Display changes are saved by the compositor, so they "
                + "survive a logout."
            color: Services.Colors.color8
            font.pixelSize: 13
        }

        SectionHeader { text: "Primary screen"; showDivider: false }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.screenNames
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    readonly property bool active:
                        Services.PawConfig.get("appearance.startupoutput", "") === modelData
                    readonly property bool cursor: root.cursorIndex === index
                    width: nameText.implicitWidth + 28
                    height: 34
                    radius: Services.Ui.radiusSm
                    color: active ? Services.Colors.accent : Services.Colors.color0
                    border.color: active || cursor ? Services.Colors.accent
                                                   : Services.Colors.color8
                    border.width: cursor ? 2 : 1

                    StyledText {
                        id: nameText
                        anchors.centerIn: parent
                        text: parent.modelData
                        font.pixelSize: 14
                        color: parent.active ? Services.Colors.background
                                             : Services.Colors.foreground
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Services.PawConfig.set("appearance.startupoutput",
                            parent.active ? "" : parent.modelData)
                    }
                }
            }
        }

        StyledText {
            Layout.fillWidth: true
            visible: Services.PawConfig.get("appearance.startupoutput", "") === ""
            text: "No screen chosen — paw will use the first one that connects."
            color: Services.Colors.color8
            font.pixelSize: 12
            font.italic: true
        }

        // Without a backend this page is still useful for the primary screen
        // alone, so it says what is missing rather than hiding itself.
        StyledText {
            Layout.fillWidth: true
            visible: !Services.Outputs.available
            wrapMode: Text.WordWrap
            topPadding: 8
            text: "Resolution, refresh rate, scale and orientation need either paw2 or "
                + "wlr-randr, and "
                + "neither is here:\n    sudo dnf install wlr-randr\n"
                + "Everything else on this page works without them."
            color: Services.Colors.yellow
            font.pixelSize: 13
        }

        Repeater {
            id: outputRows
            model: Services.Outputs.available ? Services.Outputs.list : []

            delegate: ColumnLayout {
                id: outRow
                required property var modelData
                required property int index
                Layout.fillWidth: true
                spacing: 6

                // activateAt() opens one of these, and a Repeater delegate has
                // no id from outside.
                property alias resDrop: resolutionDrop
                property alias rateDrop: refreshDrop
                property alias scaleSlider: scalingSlider
                property alias transformDrop: orientationDrop
                // Cursor positions: chips first, then each output's controls.
                readonly property int selBase: root.screenNames.length
                    + index * root.controlsPerOutput

                readonly property var configured: root.configuredMonitor(modelData.name)
                /// paw2 keeps VRR in its config, so it is offered wherever paw2
                /// is running. Whether the display can actually do it is not
                /// something paw2 reports, so "never" stays a safe answer.
                readonly property bool vrrSupported: Services.Outputs.paw
                readonly property string vrr: configured && configured.vrr
                    ? configured.vrr : "automatic"

                /// Keyboard equivalent of clicking the segmented control.
                function cycleVrr() {
                    if (!vrrSupported)
                        return;
                    const order = ["never", "automatic", "always"];
                    const next = (order.indexOf(vrr) + 1) % order.length;
                    Services.PawConfig.setMonitor(modelData.name, "vrr", order[next]);
                }

                // Sizes, newest first — one dropdown for "how big", a second
                // for "how fast", which is how people think about it. A single
                // 40-entry list is not.
                readonly property var sizes: {
                    let seen = [], out = [];
                    for (const m of modelData.modes) {
                        const key = m.width + "x" + m.height;
                        if (seen.indexOf(key) < 0) { seen.push(key); out.push(key); }
                    }
                    return out;
                }
                readonly property var current:
                    modelData.modes.find(m => m.current) || null
                property string chosenSize: current
                    ? current.width + "x" + current.height
                    : (sizes[0] || "")
                // Modes that report no rate (virtual outputs, some adaptors)
                // would read as "0.000 Hz"; drop them and the dropdown hides
                // itself.
                readonly property var rates: modelData.modes
                    .filter(m => (m.width + "x" + m.height) === chosenSize
                        && m.refresh > 0)
                    .map(m => m.refresh.toFixed(3) + " Hz")

                SectionHeader {
                    text: outRow.modelData.name
                        + (outRow.modelData.description
                           && outRow.modelData.description !== outRow.modelData.name
                            ? "  ·  " + outRow.modelData.description : "")
                    size: 14
                }

                StyledDropdown {
                    id: resolutionDrop
                    Layout.fillWidth: true
                    cursor: root.cursorIndex === outRow.selBase
                    label: "Resolution"
                    labelWidth: 110
                    model: outRow.sizes
                    currentValue: outRow.chosenSize
                    onSelected: v => {
                        outRow.chosenSize = v;
                        // Apply at the fastest rate this size offers; the rate
                        // dropdown below refines it.
                        const best = outRow.modelData.modes
                            .filter(m => (m.width + "x" + m.height) === v)
                            .sort((a, b) => b.refresh - a.refresh)[0];
                        if (best)
                            Services.Outputs.apply(outRow.modelData.name,
                                { mode: best.label });
                    }
                }

                StyledDropdown {
                    id: refreshDrop
                    Layout.fillWidth: true
                    visible: outRow.rates.length > 0
                    cursor: root.cursorIndex === outRow.selBase + 1
                    label: "Refresh"
                    labelWidth: 110
                    model: outRow.rates
                    currentValue: outRow.current && outRow.current.refresh > 0
                        ? outRow.current.refresh.toFixed(3) + " Hz" : ""
                    onSelected: v => Services.Outputs.apply(outRow.modelData.name,
                        { mode: outRow.chosenSize + "@" + v.replace(" Hz", "") + "Hz" })
                }

                StyledSlider {
                    id: scalingSlider
                    Layout.fillWidth: true
                    cursor: root.cursorIndex === outRow.selBase + 2
                    label: "Scale"
                    labelWidth: 110
                    from: 50
                    to: 200
                    stepSize: 5
                    suffix: "%"
                    value: (outRow.modelData.scale ?? 1) * 100
                    onMoved: v => Services.Outputs.apply(outRow.modelData.name,
                        { scale: v / 100 })
                }

                StyledDropdown {
                    id: orientationDrop
                    Layout.fillWidth: true
                    cursor: root.cursorIndex === outRow.selBase + 3
                    label: "Orientation"
                    labelWidth: 110
                    model: root.transformLabels
                    currentValue: root.transformLabel(outRow.modelData.transform ?? "normal")
                    onSelected: v => Services.Outputs.apply(outRow.modelData.name,
                        { transform: root.transformValue(v) })
                }

                // Written to paw2's config rather than applied live: it is a
                // per-monitor policy paw2 reads at startup, and wlr-randr has
                // no way to express "automatic" in any case.
                SegmentedControl {
                    Layout.fillWidth: true
                    enabled: outRow.vrrSupported
                    cursor: root.cursorIndex === outRow.selBase + 4
                    label: "Adaptive sync"
                    labelWidth: 110
                    model: ["never", "automatic", "always"]
                    currentValue: outRow.vrr
                    onSelected: v => Services.PawConfig.setMonitor(
                        outRow.modelData.name, "vrr", v)
                }

                StyledText {
                    Layout.fillWidth: true
                    Layout.leftMargin: 122
                    wrapMode: Text.WordWrap
                    text: outRow.vrrSupported
                        ? "Automatic turns VRR on for a fullscreen window. Takes "
                          + "effect when paw2 next starts."
                        : "Adaptive sync needs paw2."
                    color: Services.Colors.color8
                    font.pixelSize: 12
                }
            }
        }
    }
}
