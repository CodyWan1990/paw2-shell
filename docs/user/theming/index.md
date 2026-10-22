# Theme

Controls the active palette source and the dark/light modes used by paw2-shell and by your apps.

```toml
[theme]
mode              = "dark"        # dark | light | auto
shell_mode        = "follow"      # follow | dark | light | auto
source            = "builtin"     # builtin (presets) | wallpaper | custom
builtin           = "Tokyo-Night" # preset name when source = "builtin"
custom_palette    = "MyPalette"   # file name (without .json) when source = "custom"
wallpaper_scheme  = "m3-content"  # generator used when source = "wallpaper"
pure_black_dark   = false         # replace dark-mode backgrounds with pure black
sync_color_scheme = true          # sync the GNOME color-scheme preference with mode
```

Palette changes apply live when the resolved config changes, whether through Settings or edits to `~/.config/paw2-shell/*.toml`. Wallpaper-derived themes re-resolve automatically when the default wallpaper changes. See [Palette](palette.md) for color roles, custom palette files, and terminal colors.

---

## `mode`

`dark` or `light` selects the palette variant directly. `auto` switches between them using the shared sunrise/sunset
schedule in [`[location]`](../services/location.md) (same rules as [Night Light](../services/night-light.md)): sunrise and
sunset computed from your coordinates, or your own `sunset` / `sunrise` times when
[`custom_schedule = true`](../services/location.md#custom-schedule). Night light does not need to be enabled for theme
auto to follow the schedule. When the schedule cannot be resolved, `auto` uses the light (day) palette until a valid
schedule is available.

This is the mode your apps run in: it selects the palette variant written by
[templates](templates.md), the GNOME `color-scheme` preference (when
[`sync_color_scheme`](#sync_color_scheme) is on), and the mode reported to
[`theme_mode_changed`](../automation/hooks.md) hooks. `theme-mode-toggle`, `theme-mode-set`, the control center
shortcut, and the theme mode bar widget all act on this key.

## `shell_mode`

paw2-shell's own mode: bars, panels, the launcher, and wallpaper directory selection.

| Value | Description |
|-------|-------------|
| `follow` | Track `mode`, so paw2-shell and your apps switch together (default). |
| `dark` / `light` | Pin paw2-shell to that variant while `mode` keeps driving apps. |
| `auto` | Switch paw2-shell on the `[location]` schedule, independently of `mode`. |

Pinning it gives combinations such as a permanently dark bar with light apps, or a dark bar while apps follow the
day/night schedule. Templates and the GNOME `color-scheme` preference never follow `shell_mode`; a `shell_mode`-only
change repaints paw2-shell without rewriting a single template.

## `pure_black_dark`

When `true`, replaces dark-mode background roles with pure black while preserving the palette's foreground and accent
colors. It has no effect in light mode. Accessibility `high_contrast` also forces this behavior for dark themes.

## `sync_color_scheme`

When `true` (default), paw2-shell writes `org.gnome.desktop.interface color-scheme` (`prefer-dark` or `prefer-light`,
through `gsettings` or `dconf`) whenever templates are applied for `mode`. Browsers, Electron, Qt, and GTK apps read this
preference through the desktop portal to pick their dark or light look, so it works without any template enabled. Set
it to `false` to manage the preference yourself, for example to keep light apps under a dark `mode`.

### Related IPC

For theme mode and palette commands, see
[Media & UI → Theme](../ipc/media-and-ui.md#theme).

---

## `source`

| Value | Description |
|-------|-------------|
| `builtin` | Use a preset: one compiled into the binary or shipped in `assets/palettes/`. Settings calls this source **Presets**. |
| `wallpaper` | Generate a palette from the current default wallpaper each time it changes. |
| `custom` | Load a palette JSON file from the paw2-shell config directory's `palettes/` folder. |

---

## `builtin`

Names the preset to load when `source = "builtin"`. Unknown names fall back to `Tokyo-Night`.

Compiled presets: `Ayu`, `Catppuccin`, `Dracula`, `Eldritch`, `Gruvbox`, `Kanagawa`, `Nord`, `Rosé Pine`, `Tokyo-Night`.

Shipped presets (`assets/palettes/*.json`): `Catppuccin Frappe`, `Catppuccin Macchiato`, `Everforest`, `Flexoki`, `GitHub`,
`Gruvbox Material`, `Kanagawa Dragon`, `One`, `Oxocarbon`, `Rose Pine Moon`, `Solarized`, `Tokyo Night Moon`,
`Tokyo Night Storm`, `Tomorrow`. A new preset is a palette JSON file in that folder, in the same format as
[custom palettes](palette.md).

Configs from before the presets merge that used `source = "community"` are migrated to `builtin` with the same name.

---

## `custom_palette`

Names a palette file in the paw2-shell config directory's `palettes/` folder (honoring `PAW2_SHELL_CONFIG_HOME`, then `XDG_CONFIG_HOME`). The value is the file name without the `.json` extension - for example, `custom_palette = "MyPalette"` loads `~/.config/paw2-shell/palettes/MyPalette.json` with the default config location.

The Settings GUI lists palette files found in the directory when `source = "custom"` is selected. If the file is missing or invalid, the shell falls back to the `Tokyo-Night` preset.

See [Palette](palette.md) for the custom palette JSON format.

---

## `wallpaper_scheme`

Picks the generator when `source = "wallpaper"`:

| Value | Description |
|-------|-------------|
| `m3-tonal-spot` | M3 default - balanced tones anchored on the seed color |
| `m3-content` | M3 for content-forward UIs - higher chroma |
| `m3-fruit-salad` | M3 with playful cross-hue accents |
| `m3-rainbow` | M3 spanning the full wheel for multi-accent layouts |
| `m3-monochrome` | M3 collapsed to a single hue |
| `vibrant` | Custom - saturated and high-contrast |
| `faithful` | Custom - stays close to the source image |
| `soft` | Custom - source-faithful with gentler saturation |
| `dysfunctional` | Custom - deliberately off-kilter |
| `muted` | Custom - low-saturation |

## Where next?

- [Palette](palette.md) covers color roles, custom palette files, and terminal colors.
- [App Theming](app-theming.md) covers applying paw2-shell colors to app config files.
- [Template Reference](templates.md) covers template syntax, tokens, filters, hooks, and CLI rendering.
- [FAQ: external wallpaper apps](../getting-started/faq.md#how-do-i-use-paw2-shell-colors-with-an-external-wallpaper-app) covers wallpaper-driven colors when another tool displays the wallpaper.
