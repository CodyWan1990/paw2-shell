# How configuration works

paw2-shell can be configured in two ways: files you write yourself, and settings the app saves for you.

For most people, the important rule is simple: put your own configuration in `~/.config/paw2-shell/`. If something changed in the GUI and now seems to ignore your file, check `~/.local/state/paw2-shell/settings.toml`.

## The two layers

Your hand-written config lives in:

- `$PAW2_SHELL_CONFIG_HOME/paw2-shell/`
- `$XDG_CONFIG_HOME/paw2-shell/`
- or `~/.config/paw2-shell/`

paw2-shell reads every `*.toml` file in that folder, sorts them alphabetically, and merges them into one config. A single `config.toml` is the easiest setup:

```toml
# ~/.config/paw2-shell/config.toml
[theme]
mode = "dark"

[bar.default]
position = "top"
```

You can also split things up if you like tidy dotfiles:

```text
~/.config/paw2-shell/
├── bar.toml
├── theme.toml
└── widgets.toml
```

GUI-managed overrides live in:

- `$PAW2_SHELL_STATE_HOME/paw2-shell/settings.toml`
- `$XDG_STATE_HOME/paw2-shell/settings.toml`
- or `~/.local/state/paw2-shell/settings.toml`

paw2-shell writes this file when you change settings through the UI, setup flows, IPC-backed controls, and other runtime actions that need to persist. If the file is a symbolic link, paw2-shell writes through to the link target and keeps the link in place.

`PAW2_SHELL_CONFIG_HOME` and `PAW2_SHELL_STATE_HOME` have the same "home root" shape as the XDG variables. For example, `PAW2_SHELL_CONFIG_HOME=/tmp/profile` reads `/tmp/profile/paw2-shell/`. Use these paw2-shell-specific variables when you want a separate shell profile without changing the environment inherited by apps launched from paw2-shell.

## Including extra files

Automatic loading only sees `*.toml` files sitting directly in the config folder. To pull in files from subdirectories, load a specific file, or control load order without renaming files, add an `[include]` table:

```toml
# ~/.config/paw2-shell/config.toml
[include]
files = [
  "widgets/",                  # a directory: every *.toml inside it (sorted, non-recursive)
  "bars/top.toml",             # a single file
  "~/.config/shared/base.toml", # ~ , $VAR and ${VAR} are expanded
]

# the rest of this file's settings go here as usual
[theme]
mode = "dark"
```

Each entry is either a directory (which loads every `*.toml` directly inside it, sorted alphabetically) or a single file. Relative paths resolve against the directory of the file doing the including, so a file in `profiles/work.toml` can reach a shared bundle with `"../widgets/"`. Paths starting with `~`, or containing `$VAR` / `${VAR}`, are expanded.

**The including file wins.** Included files are merged first, as a reusable base, and then the file's own settings are layered on top. So a value you set directly in `config.toml` overrides the same value pulled in from an include:

```toml
[include]
files = ["bars/base.toml"]   # base.toml sets bar.top.thickness = 30

[bar.top]
thickness = 40               # this wins - the bar ends up 40
```

Includes can nest (an included file can include more files), and each file is loaded at most once - if two files include the same file, it is merged once and a cycle is detected and skipped rather than looping forever.

### Switching profiles

By default, every `*.toml` in the config folder still loads automatically alongside your includes. If you keep alternative setups in the root that should not all load at once - say `profile_a.toml` and `profile_b.toml` - set `autoload = false` in your entry file. Then **only** the file(s) that set it (plus whatever they include) load, and the other root files are ignored:

```toml
# ~/.config/paw2-shell/config.toml
[include]
autoload = false
files = ["profiles/work.toml"]
```

`autoload` only has an effect in a file loaded directly from the config folder; it is ignored in included files. `[include]` is honored in your config files only - it has no effect in the app-managed `settings.toml`.

## Which value wins?

paw2-shell loads configuration in this order:

1. Built-in defaults
2. Your `*.toml` files in the resolved config directory - for each file, anything it pulls in via `[include]` first, then the file's own settings on top
3. GUI-managed overrides in the resolved state directory's `settings.toml`

Because `settings.toml` loads last, it wins when it contains the same setting as your hand-written config layer.
When Settings writes a value that matches the parsed value from the lower layers, paw2-shell removes that redundant key
instead of keeping it as a GUI override.

> **Tip**
>
> If you are confused about why a hand-written value is not taking effect, inspect or temporarily move `settings.toml` from the resolved state directory.

## Where should I edit?

Use `~/.config/paw2-shell/` for curated config: the things you want to keep in dotfiles, copy to another machine, or manage declaratively.

Treat `~/.local/state/paw2-shell/settings.toml` as an app-managed override file. It is useful to inspect when debugging GUI changes, and safe to delete when you want to clear those overrides. Keeping it outside `~/.config` also lets the GUI save changes when your config directory is read-only, for example on NixOS. Support reports include config sources and `settings.toml`, but omit `state.toml` contents because they can contain private runtime data.

Both layers are watched for changes and hot-reloaded. If neither layer exists, paw2-shell uses its built-in defaults.

## Automatic configuration migrations

When a release changes the shape of a setting, paw2-shell migrates old values before parsing them so the running shell
keeps the intended behavior. The app-owned `settings.toml` is upgraded once and stamped with an internal
`config_version`; paw2-shell manages that marker and excludes it from exported user config.

Files under `~/.config/paw2-shell/` are never rewritten. Values that need migration are normalized temporarily on every
load, and Settings keeps a warning visible until you update the source TOML. The warning and notification identify the
source file, line, column, and exact legacy key to edit. paw2-shell sends the notification when it first finds an affected
key, when a new affected key appears, and every three days while affected values remain.

Run the validator to list every affected key, its source location, and its canonical replacement:

```sh
paw2-shell config validate
```

paw2-shell can keep using normalized values while you update a file, but migration warnings are intentionally not
permanent compatibility promises. Treat them as required config maintenance.

## File locations

Where paw2-shell keeps everything, by purpose:

| What | Where | Purpose |
|------|-------|---------|
| Your config | `~/.config/paw2-shell/config.toml` | hand-written / declarative - the base layer |
| GUI overrides | `~/.local/state/paw2-shell/settings.toml` | written by Settings; wins over your config |
| Internal UI state | `~/.local/state/paw2-shell/state.toml` | app-managed non-secret state and last-used values |
| Calendar credentials | Desktop Secret Service or an explicit CalDAV password file | CalDAV passwords |
| Encrypted storage key | Desktop Secret Service or an explicit key file | master key for encrypted clipboard history and calendar events |
| Calendar event cache | `$XDG_CACHE_HOME/paw2-shell/calendar/events.enc` | encrypted last-known-good events |
| Custom palettes | `~/.config/paw2-shell/palettes/` | your own color palette files |

It comes down to **three storage owners**:

- **Yours** - `~/.config/paw2-shell/` (settings you write).
- **paw2-shell's** - `~/.local/state/paw2-shell/` (GUI overrides and UI state).
- **Credential provider's** - durable calendar credentials and the encrypted storage master key managed by Secret Service,
  or explicit credential/key files managed by an external provisioner such as agenix. Deleting paw2-shell's state
  directory does not remove them.

Every path honors the relevant standard `XDG_CONFIG_HOME` / `XDG_STATE_HOME` / `XDG_DATA_HOME` / `XDG_CACHE_HOME`
variable (and the `PAW2_SHELL_CONFIG_HOME` / `PAW2_SHELL_STATE_HOME` / `PAW2_SHELL_DATA_HOME` overrides where applicable).

## Exporting config

Export the merged user config from the command line:

```sh
paw2-shell config export > paw2-shell-config.toml
```

Export the full effective config, including built-in defaults:

```sh
paw2-shell config export full > paw2-shell-full-config.toml
```

Export stops with an error (and writes nothing) if a config file fails to parse or an `[include]` points at a file that does not exist, so a broken config never produces a misleading partial export. (The running shell is more forgiving: it loads what it can and surfaces the problem as a notification.)

Because export writes TOML to stdout, you can pipe it into TOML-aware query tools. For example, with
[`yq`](https://github.com/mikefarah/yq):

```sh
paw2-shell config export full | yq -p toml -r '.theme.mode'
paw2-shell config export full | yq -p toml -r '.shell.offline_mode'
paw2-shell config export full | yq -p toml '.bar.default'
```

If you prefer normal `jq` filters, convert the TOML stream to JSON first:

```sh
paw2-shell config export full | yq -p toml -o json '.' | jq -r '.theme.mode'
```

The Settings actions menu includes **Export Config...**. It opens a chooser with two export modes:

- **Merged User Config** is recommended for dotfiles. It merges your explicit config files and GUI-managed overrides into one TOML file, while leaving built-in defaults implicit.
- **Full Effective Config** exports the active config snapshot, including built-in defaults. Use it for inspection or sharing an exact state, but avoid treating it as your long-term curated config because it pins defaults that could otherwise evolve.

## Validating your configuration

Run the validator to catch problems before they silently take effect:

```sh
paw2-shell config validate
```

With no argument it checks the merged configuration exactly as the shell loads it: every `*.toml` in `~/.config/paw2-shell/` (or `$PAW2_SHELL_CONFIG_HOME`), then the `settings.toml` overrides in `~/.local/state/paw2-shell/`. Pass a directory to validate just that directory's `*.toml` files instead:

```sh
paw2-shell config validate ./my-config-dir
```

Pass a file to validate only that TOML file, without scanning default locations or merging `settings.toml`. This is useful for generated configs, including NixOS-managed config files:

```sh
paw2-shell config validate ./config.toml
```

Every diagnostic is prefixed with the file, line, and column it came from, followed by the dotted config path:

```
WARN  /home/you/.config/paw2-shell/bar.toml:14:10: bar.main.foo: unknown setting
ERROR /home/you/.config/paw2-shell/widgets.toml:22:12: widget.clock.timezone: unknown timezone "Europe/Berln"
```

With several config files in play (or `settings.toml` overriding one of them), the reported file is the one whose definition actually won the merge.

It reports two severities:

- **Errors** - problems that stop the configuration from loading correctly: TOML syntax errors, values that fail to parse such as an invalid color, invalid cross-field combinations such as a keyboard layout with both its glyph and label hidden, an `[include]` whose `files` entry points at a path that does not exist, or a malformed `[include]` table (`autoload` that is not a boolean, `files` that is not a list of strings). These exit with status `1`.
- **Warnings** - things the shell loads fine but ignores, adjusts, or migrates in memory: unknown/obsolete sections and settings (misspelled or left over from an older version), values with a canonical replacement, values outside the allowed range (clamped), and invalid enum choices. Warnings are advisory and do **not** fail the check. Unknown widget and desktop-widget settings are checked against each widget type's known options.

A config with only warnings still prints `✓ Config is valid` and exits `0` (with the warnings listed), so unknown keys won't break a pre-commit hook or CI step - they're surfaced as cleanup hints. Only genuine errors exit `1`.

The shell log carries the same prefix, and the on-screen config-error notification puts the location in its title:

```
config.toml:22:12
Error: widget.clock.timezone: unknown timezone "Europe/Berln"
```

There the file is shown relative to your config directory (or as a bare file name for `settings.toml`), so a bad value points straight at the line that set it. When the position cannot be determined, the notification falls back to a `Config error` title.

## Where next?

- [Shell](shell.md) covers global behavior like fonts, scale, shadows, OSD, and keybinds.
- [Hooks](../automation/hooks.md) covers advanced event-driven automation.
- [Bars](../bar/index.md) and [Widgets](../bar/widgets/index.md) cover the top or side bar and everything inside it.
- [Dock](../dock/index.md) covers the optional app dock.
- [Theme](../theming/index.md) covers theme mode and palette source selection.
- [Palette](../theming/palette.md) covers color roles, custom palette files, and terminal colors.
- [App Theming](../theming/app-theming.md) covers generated themes for other apps.
- [Template Reference](../theming/templates.md) covers template syntax, tokens, filters, and hooks.
- [Wallpaper & backdrop](../desktop/wallpaper.md) covers wallpapers, per-monitor folders, automation, and overview backdrop.
- Service pages cover [audio](../services/audio.md), [brightness](../services/brightness.md), [calendar](../services/calendar.md), [idle](../services/idle.md), [night light](../services/night-light.md), [notifications](../services/notifications.md), and [weather](../services/weather.md).
- [IPC](../ipc/index.md) lists commands for keybinds, scripts, automation, and hooks.
