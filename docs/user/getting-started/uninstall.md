# Uninstall

From the source tree you built in, remove everything `meson install` put in place:

```bash
ninja -C build-release uninstall
```

Prefix the command with `sudo` if you installed into a system prefix such as `/usr/local`.

## Cleaning up

paw2-shell keeps its settings, state and cache in the XDG directories:

```bash
rm -rf ~/.config/paw2-shell ~/.local/state/paw2-shell ~/.cache/paw2-shell
```

Remove the `paw2-shell` entry from `[autostart]` in your paw2 config as well.

### Generated templates

App themes written by enabled templates stay behind. Turn the templates off in **Settings → Templates** before uninstalling and their undo hooks remove the files; otherwise delete the `paw2-shell.*` files from each app's config folder yourself.
