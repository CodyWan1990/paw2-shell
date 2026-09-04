# paw2-shell

paw2-shell is an optional desktop shell for the [paw2](../paw2/) Wayland
compositor. It provides a top bar, dock, app launcher, notifications, clipboard
history, screenshots, wallpapers, themes, and a graphical settings app.

It is a [Quickshell](https://quickshell.org) configuration rather than a
compiled application. Paw2 works without it, and paw2-shell also starts on
other wlroots-style compositors; controls that require paw2 simply stay hidden.

This project is still evolving. Expect occasional rough edges and Quickshell
API changes before 1.0.

## Features

- Configurable top bar and application dock
- App launcher, window switcher, emoji picker, and power menu
- Notifications, media controls, audio, brightness, battery, and system stats
- Clipboard history and region screenshots
- Theme and wallpaper pickers
- Mouse and keyboard navigation throughout the settings UI
- Live graphical configuration for paw2 displays, input, appearance, and keys

## Install

You need a Wayland compositor, Git, and Quickshell 0.3 or newer. Fedora is the
tested path. Other distributions can follow the
[official Quickshell installation guide](https://quickshell.org/docs/v0.3.0/guide/install-setup/)
and install equivalent optional tools.

### 1. Install Quickshell

On Fedora, Quickshell's official documentation recommends its COPR for Fedora
releases where the package is not yet in the main repositories:

```sh
sudo dnf copr enable errornointernet/quickshell
sudo dnf install quickshell
```

### 2. Install paw2-shell

Clone the repository directly into Quickshell's named-config directory:

```sh
git clone https://github.com/codywan-kenobi/paw2-shell.git \
  ~/.config/quickshell/paw2-shell
```

If that directory already exists, update it instead of cloning over it:

```sh
git -C ~/.config/quickshell/paw2-shell pull --ff-only
```

The repository includes a neutral `settings.json` with the starter bar layout
and blank weather location. Customize it through the shell's Settings window.

### 3. Start the shell

Run it once from a terminal:

```sh
qs -c paw2-shell
```

The first-run screen walks through displays, colors, wallpapers, default apps,
keybinds, and optional dependencies. You can reopen it later from
**Settings → General**.

To start paw2-shell with paw2, add this to paw2's `config.toml`:

```toml
[autostart]
commands = [
  ["qs", "-c", "paw2-shell"],
]
```

## Optional tools

The shell still starts when these are missing. The first-run screen explains
which feature each tool enables.

| Package | Feature |
| --- | --- |
| `cliphist`, `wl-clipboard` | Clipboard history and copying |
| `grim`, `slurp` | Screenshots and region selection |
| `swaybg` | Wallpapers |
| `wtype` | Typing a selected emoji into the active app |
| `brightnessctl` | Brightness controls |
| `wlr-randr` | Monitor controls when not running paw2 |
| `jq` | Package-update count |
| `libnotify` | Notifications from helper scripts |

Install all of them on Fedora with:

```sh
sudo dnf install wlr-randr cliphist wl-clipboard wtype grim slurp \
  swaybg jq brightnessctl libnotify
```

When using clipboard history with paw2, `wl-paste` must be allowed under paw2's
`[security] privileged_clients`. The paw2 example configuration documents the
required entry.

## Keybindings

Panels are opened through Quickshell IPC. Add any bindings you want under
paw2's `[bindings.commands]` table:

```toml
"Super+Space" = ["qs", "-c", "paw2-shell", "ipc", "call", "shell", "launcher"]
"Super+v"     = ["qs", "-c", "paw2-shell", "ipc", "call", "shell", "clipboard"]
"Print"       = ["qs", "-c", "paw2-shell", "ipc", "call", "shell", "screenshot"]
```

Available actions are `launcher`, `clipboard`, `powermenu`, `wallpaper`,
`themes`, `settings`, `screenshot`, `windowswitcher`, `welcome`, and `emoji`.

## Using another compositor

Without `$PAW2_SOCKET`, paw2-shell hides workspaces, the layout and window
switchers, the window title, logout, and compositor settings. The launcher,
dock, bar, notifications, tray, media controls, clipboard, screenshots,
wallpapers, themes, and system widgets remain available.

Monitor settings fall back to `wlr-randr` where the compositor supports it.

## Settings and safety

Shell preferences are saved in
`~/.config/quickshell/paw2-shell/settings.json`. Its shipped weather location
is blank. The weather API key is kept separately in the gitignored
`weather.conf` and is never distributed.

Paw2-specific settings are written to generated drop-ins under
`~/.config/paw2/config.d/`; paw2-shell never rewrites your hand-edited
`config.toml`. Paw2 remains responsible for defaults, validation, and reloads.

Optional external theme syncing can modify Kitty and GTK configuration. Before
touching an existing file, the shell creates a `.pawbak` backup, and the
setting can be disabled from the Themes screen.

### Keyboard navigation

In Settings, use **Up/Down** to move, **Left/Right** to adjust a value, and
**Enter** to activate or open a control. **Tab** moves between the sidebar and
the current page, **Ctrl+Up/Down** changes section, and **Escape** backs out or
closes the window.

## Themes and wallpapers

Themes are drop-in JSON files in [`themes/`](themes/). You can also create and
edit themes from the graphical Themes screen.

Wallpapers are intentionally not included. Choose a wallpaper directory in
**Settings → General**, then use the Wallpapers screen to apply an image or
associate it with the current theme.

Weather uses a free OpenWeatherMap API key. Enter it through
**Settings → Weather**; it is stored locally and never added to
`settings.json`.

## Development

The main entry point is `shell.qml`. Shared state lives in `services/`, reusable
QML in `components/`, complete surfaces in `panels/`, bar items in `widgets/`,
and operating-system helpers in `scripts/`.

Run the complete static and staged test suite before committing:

```sh
./scripts/check.sh
```

For a focused live test, run paw2 nested and point Quickshell at this checkout:

```sh
cd ../paw2
cargo run -- --nested sh -c 'qs -p ../paw2-shell'
```

## License

A project license has not yet been chosen. Quickshell itself is LGPL-3.0.
