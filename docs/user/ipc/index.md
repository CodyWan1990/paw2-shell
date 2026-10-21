# IPC & Keybinds

IPC commands let you control paw2-shell from a terminal, a compositor keybind, a script, or a hook.

All commands use this shape:

```sh
paw2-shell msg <command>
```

For example:

```sh
paw2-shell msg dock-toggle
paw2-shell msg volume-up
paw2-shell msg panel-toggle launcher
paw2-shell msg screenshot-region
```

> **Note**
>
> These pages list commands exactly as you run them. The same `paw2-shell msg ...` form is used in terminals, compositor keybinds, hooks, hot-corner commands, and idle command fields.

## Command lists

- **[Shell](shell.md)** — Status, config reload, settings, window switcher, and session actions.
- **[Surfaces](surfaces.md)** — Bar, panels, dock, desktop widgets, and lockscreen widgets.
- **[Media & UI](media-and-ui.md)** — Notifications, clipboard, media controls, wallpaper, theme, and screenshots.
- **[System Controls](system-controls.md)** — Volume, microphone, brightness, night light, Wi-Fi, Bluetooth, caffeine, power profile, and display power.

## Compositor keybinds

A compositor keybind only needs to launch a paw2-shell command. The command part is compositor-agnostic:

```sh
paw2-shell msg panel-toggle launcher
paw2-shell msg panel-toggle control-center
paw2-shell msg settings-toggle
paw2-shell msg volume-up
```

Bind them in **Settings → Shortcuts**, or under `[bindings.commands]` in your paw2 config.


## Discovering commands

Not sure what else you can control? You can see all IPC commands via this terminal command:

```bash
paw2-shell msg --help
```

## Using IPC in config

Config command fields run through the shell. Use the canonical IPC CLI when a command needs to call paw2-shell, and compose it with other shell commands when needed:

```toml
[idle.behavior.custom]
timeout = 660
action = "command"
command = "paw2-shell msg session lock && notify-send 'Session locked'"

[hooks]
started = "paw2-shell msg panel-toggle launcher"
```
