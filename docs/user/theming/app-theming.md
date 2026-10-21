# App Theming

paw2-shell can render the resolved theme colors into external app config files whenever the palette changes. This lets terminals, editors, browsers, launchers, and other apps follow the same colors as the shell.

Templates always render the variant selected by [`[theme].mode`](index.md#mode), never
[`shell_mode`](index.md#shell_mode), so pinning paw2-shell to dark keeps your apps on whatever `mode` resolves to.

To rerender enabled templates for the current palette without changing the theme, use
[Media & UI → Theme](../ipc/media-and-ui.md#theme).

## Built-in Templates

Built-in templates ship with paw2-shell. Enable the built-in catalog, then opt in to the template IDs you want:

```toml
[theme.templates]
enable_builtin_templates = true
builtin_ids              = []        # opt-in; run: paw2-shell theme --list-templates
```

`builtin_ids` selects which shipped templates run. An empty array means no built-in templates are applied.

Disabling a built-in template removes its generated paw2-shell theme file and undoes any paw2-shell-owned include or theme
selection that the template added to the application's config. Disabling `enable_builtin_templates` performs the same
cleanup for every selected built-in template.

Application configs must be writable for paw2-shell to remove those integrations. For declaratively managed configs such
as Home Manager symlinks into the Nix store, remove the paw2-shell include or theme selection from the declarative source.

When an application has no user config, a built-in template creates one that includes the paw2-shell theme. Alacritty,
foot and kitty stop reading their other config locations once a user config exists, so the
template skips this step and logs a warning when another config is present, for example `/etc/xdg/foot/foot.ini`
written by the NixOS `programs.foot.settings` option. Add the include named in the warning to that config to apply the
paw2-shell theme.

List available built-in and configured template IDs:

```sh
paw2-shell theme --list-templates
```

## User Templates

User templates are declared directly in paw2-shell config:

```toml
[theme.templates.user.my_app]
input_path  = "$XDG_CONFIG_HOME/paw2-shell/templates/my-app.css"
output_path = "$XDG_CONFIG_HOME/my-app/theme.css"
post_hook   = "my-app --reload-theme"
```

Defining `[theme.templates.user.<id>]` enables that user template. Set `enabled = false` on an entry to keep it available but inactive.

Relative paths resolve from the paw2-shell config directory. A leading `$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `$XDG_STATE_HOME`, or `$XDG_CACHE_HOME` token is expanded using the XDG base directory defaults when the matching environment variable is not set.

User template entries support the same fields documented in [Template Entries](templates.md#template-entries), including `input_path_modes`, `output_path_dynamic`, `colors_to_compare`, `compare_to`, `pre_hook`, `post_hook`, `post_action`, and `index`.

## Config Location

Put dotfile-managed templates in `~/.config/paw2-shell/*.toml`:

```toml
# ~/.config/paw2-shell/templates.toml
[theme.templates]
enable_builtin_templates   = true
builtin_ids                = []

[theme.templates.user.my_app]
input_path  = "$XDG_CONFIG_HOME/paw2-shell/templates/my-app.css"
output_path = "$XDG_CONFIG_HOME/my-app/theme.css"
```

The same tables are valid in `settings.toml` because it is the final config overlay, but that file is managed by the GUI and runtime actions. Use `~/.config/paw2-shell/` for templates you want to keep in dotfiles.

## Template Reference

For template syntax, color tokens, filters, hooks, dynamic output paths, and CLI rendering, see the [Template Reference](templates.md).
