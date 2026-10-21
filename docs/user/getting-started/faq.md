# FAQ

Find answers to common setup and troubleshooting questions for paw2-shell.

## Setup

### Does paw2-shell support touchscreen scrolling?

Yes. Tap controls normally. In a scrollable view, drag vertically over rows, buttons, or other controls to scroll; a fast release continues with a fling. On a vertical page, a horizontal drag remains with an axis-specific control such as a slider.

Touch dragging requires the compositor to send `wl_touch.motion` events. If taps work but dragging never moves content, run `wev` and drag inside its window. A trace containing only `wl_touch.down` and `wl_touch.up` means the compositor or input stack is not forwarding touch motion to clients.


### How do I start paw2-shell automatically?

Add it to `[autostart]` in your paw2 config. See [Running paw2-shell](running-the-shell.md).


## Configuration

### Where should I put my config, and why is my setting ignored?

If you only use Settings, you normally do not need to edit config files. Settings writes your changes to
`~/.local/state/paw2-shell/settings.toml`.

If you write config by hand, put it in `~/.config/paw2-shell/`. paw2-shell reads every `*.toml` file in that directory,
sorted alphabetically, and merges them.

Settings changes load after hand-written config, so they win when both layers define the same setting. If a value in your
file looks ignored, check Settings first, or inspect `~/.local/state/paw2-shell/settings.toml`.

See [How configuration works](../configuration/index.md) for the full load order and export options.



### How does paw2-shell resolve application icons?

paw2-shell resolves application icons through GTK's icon-theme support. The dock, taskbar, launcher, and other shell surfaces use the active GTK icon theme; configuring an icon theme only in a Qt control center does not change paw2-shell's application icons.

Configure and apply the icon theme through your system's GTK settings, for example with `nwg-look`.


### Why do two copies of the same widget share settings?

Widget settings are attached to the widget name used in the bar list. If you place the same name twice, both entries use
the same configuration.

In Settings, open the Add widget selector and turn on the **Named Widget** toggle at the top before choosing the widget
type. Create one named widget for each copy you want to configure separately. Middle-click a widget to open the Settings
page for that exact instance.

In text config, give each copy its own name, then set `type` to the widget you want:

```toml
[bar.default]
end = ["sysmon-left", "sysmon-right"]

[widget.sysmon-left]
type = "sysmon"
# settings for the first sysmon

[widget.sysmon-right]
type = "sysmon"
# settings for the second sysmon
```

See [Widget definitions](../bar/widgets/index.md#widget-definitions) for more examples.


### How do I choose Monday or Sunday as the first day of the week?

There is no Settings toggle or TOML key for it.

paw2-shell follows your system locale, so set the first day of the week through your system's `LC_TIME` locale.


### How can I make the UI bigger or smaller?

Start with compositor output scaling. That is the correct way to handle HiDPI monitors and affects all Wayland clients consistently.

For paw2-shell-specific sizing in Settings:

- Use **Appearance → Interface → UI scale** for non-bar shell UI such as panels, launcher, clipboard, and settings.
- Use **Bar → Layout → Content scale** for bar widget content.
- Middle-click a bar widget to open widget-specific size settings where available.

In text config, the main keys are `shell.ui_scale`, `[bar.<name>].scale`, and `[widget.<name>].scale`.

See [Shell settings](../configuration/shell.md#shell-settings) and [Bars](../bar/index.md).


### How do I turn off blur behind a transparent bar?

paw2 blurs behind shell surfaces that ask for it, and only while its own blur is on. Turn the bar's blur off under
**Bar → Effects**, or turn blur off in paw2 (**Settings → Appearance**, or `blur = false` under paw2's `[appearance]`).


### How do I tweak the seam between the bar and attached panels?

Attached panels (like the control center) sit flush against the bar, so the line where they meet, the seam, can show a faint cut, a hairline gap, or a doubled edge depending on your compositor and display scaling. These artifacts mostly show up with a highly transparent bar, where the seam between the two surfaces has nothing opaque to hide behind. If the seam bothers you, the simplest fix is to raise `background_opacity` toward `1.0`; the settings below help when you want to keep the transparency. Three settings control how that seam looks.

**Hide or tune the cut with `panel_overlap`.** This sets how many logical pixels an attached panel is pulled into the bar so the two surfaces share one edge. The default is `1`. If you see a faint seam line, try `0`; if a gap appears, raise it; negative values push the panel away from the bar. The right value depends on the output's fractional scale, so on a mixed-scale multi-monitor setup you can set it per monitor with a `[bar.<name>.monitor.*]` override. In Settings it lives under **Bar → Layout → Advanced**.

**Embrace the cut with `contact_shadow`.** Instead of hiding the seam, this draws a dark gradient where the panel meets the bar, giving the panel a lifted-off-the-bar feel. It is independent of the bar's `shadow` and only affects attached panels; the change applies the next time a panel opens. In Settings it lives under **Bar → Effects**.

**Mind the compositor blur.** The bar and an attached panel are separate Wayland surfaces, so the compositor blurs each one on its own. With a large blur sampling offset the two blur passes don't line up at the seam, and the gap between them becomes visible. If the seam looks off with blur, lower your compositor's blur size/passes (or its sampling offset) rather than chasing it with `panel_overlap`.

```toml
[bar.default]
panel_overlap  = 0      # 1 is default; 0 removes the overlap, negative pushes the panel away
contact_shadow = true   # dark gradient at the seam for a lifted look
```

See [Bars](../bar/index.md) for the full per-bar and per-monitor field list.


### How can I get paw2-shell to display in my language?

paw2-shell follows your system locale by default.

In Settings, use **Appearance → Interface → Language**.

In text config, set `shell.lang`:

```toml
[shell]
lang = "fr"
```

If a translation is not available for that language, untranslated strings fall back to English.
Language values may use BCP-47 tags such as `pt-BR` and `zh-Hans`, or POSIX locale names such as `pt_BR.UTF-8`.
For Chinese locales without an explicit script, paw2-shell derives the script from the region: for example `zh_CN`
matches `zh-Hans`.


### How do I use paw2-shell colors with an external wallpaper app?

If you manage wallpapers with another tool such as waypaper, swww, or hyprpaper, let that app draw the wallpaper and let
paw2-shell use the image only for color extraction.

In Settings:

1. Turn off **Wallpaper → General → Enabled**.
2. Set **Appearance → Theme → Palette source** to **Wallpaper**.

In text config:

```toml
# Disable paw2-shell wallpaper display
[wallpaper]
enabled = false

# Generate colors from the current wallpaper path
[theme]
source           = "wallpaper"
wallpaper_scheme = "m3-content"
```

After your wallpaper app sets a new image, tell paw2-shell which file to use for color extraction:

```sh
paw2-shell msg wallpaper-set ~/Pictures/Wallpapers/new-wallpaper.png
```

paw2-shell extracts colors from that image, generates a palette with the configured scheme, and cross-fades the shell to the new colors even though it is not displaying the wallpaper itself. The path is persisted across restarts.


## Troubleshooting

### Why aren't my keybinds working?

Check these points first:

1. Make sure paw2-shell is running.
2. Shell commands use the form `paw2-shell msg ...`; test the command in a terminal first.
3. Bind it in **Settings → Shortcuts**, which writes paw2's `config.d/keybinds.toml`, or under
   `[bindings.commands]` in your own paw2 config.

See the [IPC overview](../ipc/index.md) and [Shell commands](../ipc/shell.md).


### Why is my battery widget missing?

Battery status comes from UPower. Make sure UPower is installed and running.

On NixOS, enable:

```nix
services.upower.enable = true;
```

The battery widget defaults to the system battery. If you want to show a peripheral or second battery, middle-click the
battery widget and choose its **Device** setting. In text config, set the widget's `device` key.

See the [`battery` widget docs](../bar/widgets/battery.md).


### Why won't an app appear in the launcher or dock?

paw2-shell discovers desktop applications from `.desktop` files in standard application directories. If an app is missing, verify that it installs a desktop entry under a path such as:

- `/usr/share/applications/`
- `~/.local/share/applications/`
- `$HOME/.nix-profile/share/applications/`
- `/etc/profiles/per-user/$USER/share/applications/`

For dock pins, match the desktop entry ID, `StartupWMClass`, display name, or full desktop entry path. See the [Dock](../dock/index.md) page for matching rules.


### Why are weather or Night Light not working?

Weather and Night Light both use **Settings → Location**. Set **Auto locate** or enter an **Address** there first.

For Weather, also enable **Location → Weather**. The bar `weather` widget requires the weather service to be enabled.

For Night Light, also enable **Location → Night Light**. Night Light requires compositor support for the
`wlr-gamma-control` protocol.

In text config, Weather requires `[weather] enabled = true` plus a resolved `[location]` source such as
`auto_locate = true` or `address = "..."`.

Weather, Theme `auto`, and Night Light all share the same location source.

See [Location](../services/location.md), [Theme](../theming/index.md#mode), and [Night Light](../services/night-light.md).


### Why can't I unlock my lock screen?

paw2-shell authenticates through PAM using the `login` service. Make sure `/etc/pam.d/login` exists and is valid for your distribution.

If you changed your PAM stack for fingerprint, security key, or other authentication methods, make sure those changes are present in the `login` PAM service used by paw2-shell.

After a successful PAM unlock, paw2-shell asks the same PAM stack to refresh existing credentials, such as Kerberos tickets or PAM-integrated GPG and keyring credentials.


### Does the lock screen engage before suspend?

By default yes, when the lock screen is enabled. paw2-shell holds a logind sleep delay inhibitor and locks the session on `PrepareForSleep` before allowing suspend or hibernate to proceed (including lid-close sleep). You do not need a separate idle daemon for lock-before-sleep.

To suspend without locking (for example lid close or `systemctl suspend`), set `[lockscreen].lock_before_suspend = false` in Settings → Security → Lock Screen, or in TOML. Explicit **Lock and Suspend** session/idle actions still lock first.


### Why can't NetworkManager communicate with wpa_supplicant?

If your wireless management tools (like `NetworkManager` or desktop applets) fail to communicate with `wpa_supplicant`, it is often a matter of missing D-Bus permissions or incorrect service options. The steps below allow users in the `wheel` group to interact with the wpa_supplicant daemon.

**1. Update the D-Bus configuration**

Explicitly allow the `wheel` group to send and receive signals via the `fi.w1.wpa_supplicant1` interface. Open `/etc/dbus-1/system.d/wpa_supplicant.conf` and ensure the `<policy group="wheel">` block is included before the closing `</busconfig>` tag:

```xml title="/etc/dbus-1/system.d/wpa_supplicant.conf" ins={18-22}
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
        <policy user="root">
                <allow own="fi.w1.wpa_supplicant1"/>

                <allow send_destination="fi.w1.wpa_supplicant1"/>
                <allow send_interface="fi.w1.wpa_supplicant1"/>
                <allow receive_sender="fi.w1.wpa_supplicant1" receive_type="signal"/>
        </policy>
        <policy context="default">
                <deny own="fi.w1.wpa_supplicant1"/>
                <deny send_destination="fi.w1.wpa_supplicant1"/>
                <deny receive_sender="fi.w1.wpa_supplicant1" receive_type="signal"/>
        </policy>

        <!-- Allow users in the wheel group to interact with wpa_supplicant -->
        <policy group="wheel">
                <allow send_destination="fi.w1.wpa_supplicant1"/>
                <allow receive_sender="fi.w1.wpa_supplicant1" receive_type="signal"/>
        </policy>
</busconfig>
```

**2. Configure the runit service flags**

For D-Bus control to function correctly, `wpa_supplicant` must be started with the `-u` flag. Update (or create) the service configuration file:

```sh title="/etc/sv/wpa_supplicant/conf"
OPTS="-u -M -c /etc/wpa_supplicant/wpa_supplicant.conf"
```

**3. Apply the changes**

After saving both files, restart the D-Bus daemon and the wpa_supplicant service:

```sh
# Reload D-Bus configuration
sudo pkill -HUP dbus-daemon

# Restart the wpa_supplicant runit service if you hit the service-flags issue
sudo sv restart wpa_supplicant
```


### Why does connecting to Wi-Fi hang or fail as not authorized?

Some distributions, openSUSE among them, require polkit authorization before NetworkManager adds or activates a connection. If no polkit agent answers that prompt, the request either fails as not authorized or waits until the D-Bus timeout (about 25 seconds) and then fails. paw2-shell shows a notification in both cases.

**Option 1: enable paw2-shell's polkit agent (recommended)**

Set `shell.polkit_agent = true` (Settings → Security). paw2-shell then shows the authentication prompt itself. Your system's polkit policy stays unchanged. Skip this if another desktop agent already handles prompts in your session.

**Option 2: allow NetworkManager actions without a prompt**

A polkit rule can authorize NetworkManager actions for a group. This lets every member of that group change network settings without a password, so use a group limited to trusted administrators. The group is usually `wheel` on openSUSE, Fedora, and Arch, and often `netdev` on Debian and Ubuntu; check that your user belongs to it with `id`.

```js title="/etc/polkit-1/rules.d/49-networkmanager.rules"
polkit.addRule(function (action, subject) {
  var allowed = [
    "org.freedesktop.NetworkManager.enable-disable-network",
    "org.freedesktop.NetworkManager.enable-disable-wifi",
    "org.freedesktop.NetworkManager.enable-disable-wwan",
    "org.freedesktop.NetworkManager.network-control",
    "org.freedesktop.NetworkManager.settings.modify.own",
    "org.freedesktop.NetworkManager.settings.modify.system",
    "org.freedesktop.NetworkManager.wifi.scan",
    "org.freedesktop.NetworkManager.wifi.share.open",
    "org.freedesktop.NetworkManager.wifi.share.protected",
  ];
  if (allowed.indexOf(action.id) >= 0 && subject.isInGroup("wheel")) {
    return polkit.Result.YES;
  }
});
```

polkit reloads rules automatically; no restart is needed.


### Why can't I connect to my VPN in NetworkManager?

By default, NetworkManager generates VPN system connections for the current user only. This saves secrets to `nm-applet`. If you have `nm-applet` this should work fine, however if you don't have it you will need to create the system connection for all users. 

To do this
* Open `nm-connection-editor` (Titled "Advanced Network Configuration")
* Go to the VPN tab > Authentication > Password
* On the right side of the box, click the icon to change the password type

```txt title='Example Connection'
[] Store the password only for this user
[x] Store the password for all users
[] Ask for this password every time
[] The password is not required
```

Use the option in the example to save the password to the connection or the 'ask everytime' option if you want to be prompted for every connection.
Be sure to input your password before saving.

