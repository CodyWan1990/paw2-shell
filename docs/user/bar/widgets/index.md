# Widgets

## Configure widgets

Widget definitions are optional. When a name in a widget list matches a `[widget.<name>]` entry, that entry's `type` and settings are used. When there is no match, the name itself is treated as the widget type with default settings.

Middle-click a bar widget to open Settings directly to that widget's configuration.

```toml
[widget.<name>]
type = "<widget-type>"   # defaults to <name> when omitted
scale = 1.0              # multiplies all content by the owning bar's scale
font_scale = 1.0         # additional text-only multiplier
enabled = true           # show or hide this widget on the bar
# ...widget-specific settings
```

This allows multiple named widgets with the same widget type:

```toml
[widget.clock-seconds]
type   = "clock"
format = "{:%H:%M:%S}"

[bar.default]
end = ["clock", "clock-seconds"]   # two clock widgets, different formats
```

## Shared behavior and appearance

Configure click, scroll, and thumb-button bindings under [Widget Actions](../actions.md). Configure shared capsule styling under [Bar → Widget Capsule](../index.md#widget-capsule).

## Built-in widgets

### Layout & windows

- **[`active_window`](active-window.md)** — Shows the active window icon and title for the current output.
- **[`clock`](clock.md)** — Displays the current time with configurable formats and timezone.
- **[`spacer`](spacer.md)** — Adds empty space or an optional interactive hot zone.
- **[`taskbar`](taskbar.md)** — Shows running and pinned applications, optionally grouped by workspace.
- **[`text`](text.md)** — Shows fixed custom text on the bar.
- **[`workspaces`](workspaces.md)** — Switches workspaces with regular, minimal, or Focus Hint styles.

### Audio, media & information

- **[`audio_visualizer`](audio-visualizer.md)** — Displays a horizontal audio spectrum from the current PipeWire monitor stream.
- **[`media`](media.md)** — Shows artwork and track details from the active MPRIS player.
- **[`volume`](volume.md)** — Shows and controls PipeWire output or input volume.
- **[`weather`](weather.md)** — Shows the current configured weather in the bar.

### System & hardware

- **[`battery`](battery.md)** — Shows battery charge level and state through UPower.
- **[`bluetooth`](bluetooth.md)** — Shows Bluetooth adapter state and the connected device.
- **[`brightness`](brightness.md)** — Shows and adjusts the current monitor brightness.
- **[`keyboard_layout`](keyboard-layout.md)** — Shows the current keyboard layout and cycles layouts when supported.
- **[`lock_keys`](lock-keys.md)** — Shows Caps Lock, Num Lock, and Scroll Lock state.
- **[`network`](network.md)** — Shows Wi-Fi, wired, and VPN status through NetworkManager.
- **[`power_profile`](power-profile.md)** — Shows and changes the current system power profile.
- **[`privacy`](privacy.md)** — Shows microphone, camera, and screen capture activity.
- **[`sysmon`](sysmon.md)** — Shows one configurable system resource statistic.
- **[`tray`](tray.md)** — Shows StatusNotifierItem system tray icons.

### Quick toggles

- **[`caffeine`](caffeine.md)** — Shows and controls the compositor idle inhibitor.
- **[`nightlight`](nightlight.md)** — Shows and controls scheduled or forced night light state.
- **[`theme_mode`](theme-mode.md)** — Toggles between dark and light theme mode.

### Buttons & panels

- **[`clipboard`](clipboard.md)** — Opens the clipboard history panel from the bar.
- **[`control-center`](control-center.md)** — Opens the control center panel from the bar.
- **[`custom_button`](custom-button.md)** — Shows a custom glyph, image, or label with bindable actions.
- **[`launcher`](launcher.md)** — Opens the launcher panel from the bar.
- **[`notifications`](notifications.md)** — Shows unread notifications and controls Do Not Disturb mode.
- **[`screenshot`](screenshot.md)** — Captures screen regions or displays through the bar.
- **[`session`](session.md)** — Opens the session menu panel from the bar.
- **[`settings`](settings.md)** — Opens the paw2-shell Settings window from the bar.
- **[`wallpaper`](wallpaper.md)** — Opens the wallpaper picker panel from the bar.
