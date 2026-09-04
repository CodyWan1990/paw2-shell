#!/usr/bin/env bash
# Capture a region with grim+slurp to a temp file, then open the paw2-shell
# ScreenshotTool action panel via Quickshell IPC. Requires: grim, slurp, qs.
set -euo pipefail

# $XDG_RUNTIME_DIR, not /tmp: /tmp is world-readable, so every capture would be
# readable by any local user until it is saved or discarded, and world-writable,
# so the path could be pre-created as a symlink for grim to follow. The runtime
# dir is mode 0700 and per-user. Must match ScreenshotTool.qml's screenshotPath.
out="${XDG_RUNTIME_DIR:-/tmp}/paw2-shell-screenshot.png"

geometry=$(slurp 2>/dev/null) || exit 0   # user cancelled selection
grim -g "$geometry" "$out"

# Show the action panel (matches the IpcHandler target/function in shell.qml).
qs -c paw2-shell ipc call shell screenshot
