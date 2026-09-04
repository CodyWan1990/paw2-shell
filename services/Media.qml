pragma Singleton
import QtQuick
import Quickshell.Services.Mpris

/// Shared MPRIS player selection and presentation helpers.
QtObject {
    id: root

    readonly property var players: Mpris.players.values
    readonly property var playingPlayer: {
        for (const player of players)
            if (player.playbackState !== MprisPlaybackState.Stopped)
                return player;
        return null;
    }
    readonly property var availablePlayer: playingPlayer
        ?? (players.length > 0 ? players[0] : null)

    function formatTime(seconds) {
        let total = Math.max(0, Math.floor(seconds || 0));
        let minutes = Math.floor(total / 60);
        let remainder = total % 60;
        return minutes + ":" + (remainder < 10 ? "0" : "") + remainder;
    }

    function playerIcon(identity) {
        let name = (identity || "").toLowerCase();
        if (name.includes("firefox")) return "🦊";
        if (name.includes("spotify")) return "🎧";
        if (name.includes("chromium")) return "🌐";
        if (name.includes("feishin")) return "🐟";
        if (name.includes("jellyfin")) return "🪼";
        if (name.includes("vivaldi")) return "🔴";
        if (name.includes("mpv")) return "🎬";
        if (name.includes("vlc")) return "🔶";
        if (name.includes("celluloid")) return "🎞";
        return "♫";
    }
}
