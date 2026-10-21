# Media & UI

These commands control user-facing shell features that are not tied to one persistent surface.

## Notifications

| Action | Command | Description |
|--------|---------|-------------|
| Enable Do Not Disturb | `paw2-shell msg notification-dnd-set on` | Hide current notification toasts and suppress new ones. Notification history still fills. |
| Disable Do Not Disturb | `paw2-shell msg notification-dnd-set off` | Allow new notification toasts; hidden or suppressed toasts are not replayed. |
| Toggle Do Not Disturb | `paw2-shell msg notification-dnd-toggle` | Toggle notification toast suppression. Notification history still fills. |
| Show Do Not Disturb status | `paw2-shell msg notification-dnd-status` | Print the current Do Not Disturb state. |
| Send internal notification | `paw2-shell msg notification-show <summary>` | Create a paw2-shell-origin notification with the provided summary text. |
| Invoke latest notification | `paw2-shell msg notification-invoke-latest` | Invoke the default action of the most recent active notification (raises/focuses the source app), mirroring a toast left-click. |
| Clear active notifications | `paw2-shell msg notification-clear-active` | Dismiss all currently visible active notifications. Dismissed notifications stay in control-center history. |
| Clear notification history | `paw2-shell msg notification-clear-history` | Remove all entries from notification history. |

DND enable/disable IPC commands show a short OSD for the new state.

`notification-show` accepts a summary, and CLI usage can pass body as a second argument:

```sh
paw2-shell msg notification-show "Build finished"
paw2-shell msg notification-show "Build finished" "All tests passed"
```

When a body argument is passed, the CLI forwards it as structured payload (`summary` + `body`) to the IPC handler.

For full control, pass a JSON payload object:

```sh
paw2-shell msg notification-show '{"app_name":"paw2-shell","summary":"Build finished","body":"All tests passed","urgency":"low","timeout_ms":4000,"icon":"circle-check"}'
```

Supported JSON fields:

- `app_name` (string): application label shown for the notification (default `paw2-shell`)
- `summary` (string, required for JSON mode): notification title text
- `body` (string): body text
- `urgency` (string): one of `low`, `normal`, `critical`
- `timeout_ms` (integer): auto-dismiss timeout in milliseconds (`0` keeps it persistent)
- `icon` (string): icon hint. Plain names are treated as paw2-shell glyphs (for example `circle-check`), while paths and URLs are treated as image icons.
- `category` (string): freedesktop category
- `desktop_entry` (string): desktop entry id

## Clipboard

| Action | Command | Description |
|--------|---------|-------------|
| Clear clipboard history | `paw2-shell msg clipboard-clear` | Clear clipboard history without opening the panel. Pinned entries survive, so with nothing pinned this clears everything. Does not ask for confirmation. The live selection is untouched; pasting still works until something else is copied. |
| Copy text to clipboard | `paw2-shell msg clipboard-copy <text>` | Set the clipboard selection to `<text>`. Everything after the verb is the payload, so spaces and newlines are kept verbatim. Payloads are capped at 64 KiB by the IPC transport. |
| Print clipboard text | `paw2-shell msg clipboard-text` | Print the most recent clipboard text on stdout with no trailing newline. Pinned entries and images are skipped, so this matches what pasting into a paw2-shell input would insert. Prints nothing when the history holds no text. |

## Media

| Action | Command | Description |
|--------|---------|-------------|
| Previous track | `paw2-shell msg media previous` | Ask the active MPRIS player to go to the previous item. |
| Next track | `paw2-shell msg media next` | Ask the active MPRIS player to go to the next item. |
| Play/pause toggle | `paw2-shell msg media toggle` | Toggle play/pause on the active MPRIS player. |
| Play | `paw2-shell msg media play` | Resume playback on the active MPRIS player. |
| Pause | `paw2-shell msg media pause` | Pause the active MPRIS player. No-op when nothing is playing. |
| Stop playback | `paw2-shell msg media stop` | Send the MPRIS `Stop` command to the active player and dismiss it: the player is hidden from the media widget until it starts playing again or is explicitly selected with `next-player`/`previous-player`. Use `media pause` to pause without dismissing. |
| Previous player | `paw2-shell msg media previous-player` | Switch the active player to the previous available MPRIS player and pin it. |
| Next player | `paw2-shell msg media next-player` | Switch the active player to the next available MPRIS player and pin it. |

## Wallpaper

| Action | Command | Description |
|--------|---------|-------------|
| Random wallpaper now | `paw2-shell msg wallpaper-random` | Switch to a random wallpaper immediately on all monitors. |
| Random wallpaper (one monitor) | `paw2-shell msg wallpaper-random <connector>` | Switch to a random wallpaper on the monitor named by **connector** (e.g. `DP-1`). Other monitors are unchanged. Respects per-monitor directory configuration and theme mode. |
| Next wallpaper | `paw2-shell msg wallpaper-next` | Advance to the next wallpaper in order immediately on all monitors. |
| Next wallpaper (one monitor) | `paw2-shell msg wallpaper-next <connector>` | Advance to the next wallpaper on the monitor named by **connector** (e.g. `DP-1`). Other monitors are unchanged. Respects per-monitor directory configuration and theme mode. |
| Previous wallpaper | `paw2-shell msg wallpaper-previous` | Go back to the previous wallpaper in order immediately on all monitors. |
| Previous wallpaper (one monitor) | `paw2-shell msg wallpaper-previous <connector>` | Go back to the previous wallpaper on the monitor named by **connector** (e.g. `DP-1`). Other monitors are unchanged. Respects per-monitor directory configuration and theme mode. |
| Get wallpaper | `paw2-shell msg wallpaper-get` | Print the default wallpaper path persisted in `settings.toml`. |
| Get wallpaper (one monitor) | `paw2-shell msg wallpaper-get <connector>` | Print the effective wallpaper path for the monitor named by **connector** (e.g. `DP-1`). |
| Set wallpaper (all monitors) | `paw2-shell msg wallpaper-set <path>` | Set every connected monitor and the default wallpaper, persisted to `settings.toml` (per-monitor entries plus `wallpaper.default.path`). |
| Set wallpaper (one monitor) | `paw2-shell msg wallpaper-set <connector> <path>` | Set only the monitor named by **connector** (e.g. `DP-1`). Remaining tokens are joined as the path. Other monitors are unchanged. |

Wallpaper paths accept an image path (`~` expanded) or `color:#RRGGBB` / `color:#RRGGBBAA`. Non-color paths must exist at call time.

The CLI forwards everything after the command as one string (`paw2-shell msg wallpaper-set ...`), so `paw2-shell msg wallpaper-set DP-1 /home/you/My Pictures/wall.jpg` still yields a correct path when the shell passes separate arguments after the command.

## Theme

| Action | Command | Description |
|--------|---------|-------------|
| Get theme mode | `paw2-shell msg theme-mode-get` | Print the current resolved theme mode: `dark` or `light` (from `[theme].mode` or from the `[location]` schedule when mode is `auto`). |
| Toggle theme mode | `paw2-shell msg theme-mode-toggle` | Switch between dark and light theme mode (same as the bar control when `[theme].mode` is not `auto`). |
| Set theme mode | `paw2-shell msg theme-mode-set dark` | Persist `[theme].mode` in `settings.toml` and reload. Pass `dark`, `light`, or `auto` (example shows `dark`). |
| Get color scheme | `paw2-shell msg color-scheme-get` | Print the active palette selection as two tokens: `builtin Nord`, `wallpaper m3-content`, or `custom MyScheme`. |
| Set color scheme | `paw2-shell msg color-scheme-set builtin Nord` | Persist `[theme].source` and the palette for that source, then reload. |
| Apply templates | `paw2-shell msg templates-apply` | Rerender the enabled configured theme templates for the current palette without changing theme mode or firing shell palette hooks. |

`color-scheme-set` accepts these source kinds:

- **builtin** - preset name, such as `Nord` or `Tokyo Night Storm`
- **wallpaper** - generator scheme, such as `m3-tonal-spot`, `m3-content`, `m3-fruit-salad`, `m3-rainbow`, `m3-monochrome`, `vibrant`, `faithful`, `soft`, `dysfunctional`, or `muted`
- **custom** - palette file name without `.json` under `~/.config/paw2-shell/palettes/`

## Screenshots

Requires compositor support for `wlr-screencopy-unstable-v1`. Uses the global `[shell.screenshot]` output policy (directory, filename pattern, freeze, save to file, clipboard, pipe command). See [Shell settings](../configuration/shell.md#shell-settings).

| Action | Command | Description |
|--------|---------|-------------|
| Region screenshot | `paw2-shell msg screenshot-region` | Start interactive region capture (drag to select; press **`[keybinds].cancel`** to dismiss). |
| Fullscreen screenshot | `paw2-shell msg screenshot-fullscreen` | Capture the focused monitor (compositor-selected output, active toplevel, keyboard focus, or recent pointer). |
| Fullscreen (pick monitor) | `paw2-shell msg screenshot-fullscreen pick` | Open the display picker on multi-monitor setups. On a single monitor, captures immediately. |
| Fullscreen (specific output) | `paw2-shell msg screenshot-fullscreen DP-2` | Capture one output without the picker. Pass the Wayland connector name (e.g. `DP-1`, `HDMI-A-1`) or a word-boundary match in the output description. |
| Fullscreen (all monitors) | `paw2-shell msg screenshot-fullscreen all` | One combined desktop PNG spanning every monitor. |
| Annotate a frozen screen | `paw2-shell msg screenshot-annotate` | Freeze every monitor and open the annotation editor over the frozen image. **Copy** and **Save** deliver the annotated PNG; **`[keybinds].cancel`** closes without delivering. |
| Annotate the live screen | `paw2-shell msg annotate` | Transparent drawing overlay for presentations. Apps keep updating behind the ink; **`F`** freezes the screen under it and reveals Blur, Crop, Copy, and Save. |
| Annotate an image file | `paw2-shell msg annotate ~/Pictures/shot.png` | Open an image from disk in the editor instead of the screen. No screencopy support needed. |

In picker mode, choose a connector from the top bar on any screen, or click the desktop to capture the monitor under the cursor. Errors list available connector names when the selector is unknown.

`annotate <path>` edits a file instead of the screen. The image is decoded with the shell's own loader (PNG, JPEG, GIF, WebP, JPEG XL, ICO, SVG, and the other formats used for wallpapers and icons) and opens centered at its native pixel size on the focused output, scaled down when it does not fit. EXIF orientation is applied, so camera and phone photos open upright. Relative paths resolve against the directory the command ran in, and `~` expands. The pointer button stays disabled because no cursor was captured, and **Freeze** is not offered. **Copy** and **Save** deliver the annotated PNG, and **Done** applies the `[shell.screenshot]` output policy. Saving never overwrites the source: it writes a new PNG into the screenshot folder using the configured filename pattern.

Annotation tool keys (no modifier): **`m`** move, **`b`** brush, **`h`** highlighter, **`l`** line, **`a`** arrow, **`s`** rectangle, **`o`** circle, **`t`** text, **`n`** numbering, **`u`** blur, **`e`** eraser, **`c`** crop, **`f`** freeze (live overlay), **`p`** toggle the captured pointer, **`[`** and **`]`** change the current tool's size. **`Ctrl+Z`** undoes, **`Ctrl+Shift+Z`** and **`Ctrl+Y`** redo, **`[keybinds].copy`** copies, **`[keybinds].save`** saves.

The pointer button and **P** work the same in region, fullscreen, and frozen-screen editing. `[shell.screenshot].show_cursor` sets the initial choice; toggling does not change that setting. Only the pointer recorded inside the captured area can appear. paw2-shell captures the cursor separately through `ext-image-copy-capture-v1` and composites it over the same cursor-free background for both choices. When separate cursor capture is unsupported or fails, the ordinary screenshot keeps the configured pointer choice, the button is disabled with an explanatory tooltip, and **P** shows the same explanation. In live annotation, freeze with **F** first.

`[shell.screenshot].close_on_copy` and `[shell.screenshot].close_on_save` default to `true`: a successful **Copy** or **Ctrl+C** (including a custom **`[keybinds].copy`** binding) and a successful **Save** or **Ctrl+S** (including **`[keybinds].save`**) close the editor. Set either to `false` to keep editing after that action. Failed copies and failed saves leave the editor open.

**Capture new region** in the toolbar discards the current edit, captures a fresh desktop region without the annotation overlay, and opens it in the editor with the same pointer control and remembered tool preferences. Starting from a captured image preserves the editor's current pointer choice. It is available in live, frozen, and image modes, regardless of `[shell.screenshot].annotate`. Cancelling region selection closes the capture.

Captured images have an inset **Outline** color-role border to distinguish the canvas from its surroundings. The border does not change the image size or appear in copied or saved images.

The toolbar carries a grab handle on its left edge: drag it to move the bar out of the way. Each output's position is remembered independently for later sessions and clamped to that output. Moving one toolbar does not reposition the others. Every control has a tooltip naming its shortcut. **S**, **M**, and **L** set the current tool's size to half, one, or twice its default weight, and the adjustments button next to them reveals the fine stepper (`-`, the current pixel size, `+`) for anything in between; fine values are kept, not snapped back to the nearest preset.

The live color is the swatch carrying a check mark, and the shape-fill button shows a struck-through paint roller while fill is off. Size and color are per tool, so each remembers its own weight and color. While a text caret is open, changing size or color restyles the text under the caret instead of only the next one, and touching any other control (a tool, undo, freeze, export) commits that text first.
