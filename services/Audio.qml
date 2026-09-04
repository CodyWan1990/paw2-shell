pragma Singleton

import QtQuick
import Quickshell.Services.Pipewire

/// Shared audio device listing for AudioTab and AudioSlider, backed by the native
/// Pipewire service (replaces the old pactl shell-outs). Public API is unchanged:
/// sinks/sources (objects with .name + .description), defaultSink/defaultSource
/// (name strings), setDefaultSink/Source(name), loadDevices() (no-op — Pipewire is
/// push-reactive). Volume/mute live on each node's .audio and are read/written
/// directly by AudioSlider via Pipewire.defaultAudioSink.
QtObject {
    id: root

    readonly property var sinks: Pipewire.nodes.values
        .filter(n => n.audio && n.isSink && !n.isStream)
    readonly property var sources: Pipewire.nodes.values
        .filter(n => n.audio && !n.isSink && !n.isStream && !n.name.includes(".monitor"))
    readonly property string defaultSink: Pipewire.defaultAudioSink?.name ?? ""
    readonly property string defaultSource: Pipewire.defaultAudioSource?.name ?? ""

    // No-op: AudioSlider still calls this onOpening, but Pipewire pushes updates.
    function loadDevices() {}

    function setDefaultSink(name) {
        let n = sinks.find(s => s.name === name);
        if (n) Pipewire.preferredDefaultAudioSink = n;
    }

    function setDefaultSource(name) {
        let n = sources.find(s => s.name === name);
        if (n) Pipewire.preferredDefaultAudioSource = n;
    }

    // Keep enumerated nodes bound so their name/description stay valid.
    property PwObjectTracker tracker: PwObjectTracker {
        objects: root.sinks.concat(root.sources)
    }
}
