# Surfaces

These commands open, close, or adjust paw2-shell UI surfaces.

## Bar

| Action | Command | Description |
|--------|---------|-------------|
| Show bar | `paw2-shell msg bar-show` [bar-name] [monitor-selector] | Reveal matching bar instance(s). Omit both arguments to show every bar on every output. |
| Hide bar | `paw2-shell msg bar-hide` [bar-name] [monitor-selector] | Hide matching bar instance(s) and block edge/pointer reveal until the next show/toggle. Omit both arguments to hide every bar. |
| Toggle bar | `paw2-shell msg bar-toggle` [bar-name] [monitor-selector] | Toggle visibility for matching bar instance(s). Omit both arguments to toggle every bar. Does not block edge reveal on hide. |
| Toggle reserve space | `paw2-shell msg bar-reserve-toggle` [bar-name] [monitor-selector] | Temporarily toggle reserve space for matching bar instance(s). The change affects panels opened from those bars, is not written to configuration, and resets when bar configuration reloads or the shell restarts. Omit both arguments to toggle every bar. |
| Set bar auto-hide | `paw2-shell msg bar-auto-hide-set` &lt;on\|off\|smart&gt; [bar-name] [monitor-selector] | Temporarily enable, disable or switch to smart auto-hide for matching bar instance(s). Omit `bar-name` to update every bar. |
| Set bar layer | `paw2-shell msg bar-layer-set` &lt;top\|overlay&gt; [bar-name] [monitor-selector] | Temporarily move matching bar instance(s) to the `top` or `overlay` layer-shell layer. `overlay` shows the bar above fullscreen apps; attached panels follow the bar's layer. Omit `bar-name` to update every bar. |

Use `paw2-shell msg bar-hide` when you want the bar to stay hidden until `paw2-shell msg bar-show`.

## Panels

| Action | Command | Description |
|--------|---------|-------------|
| Open panel | `paw2-shell msg panel-open <id> [context]` | Open a panel by id without toggling it closed when it is already open. Optional `context` works like `paw2-shell msg panel-toggle <id> [context]`. |
| Close panel | `paw2-shell msg panel-close [id]` | Close the active panel, or close the named panel if it is active. |
| Toggle launcher | `paw2-shell msg panel-toggle launcher [query]` | Open or close the app launcher. Optional `query` pre-fills the search input (e.g. `paw2-shell msg panel-toggle launcher "/wall"`). |
| Toggle session menu | `paw2-shell msg panel-toggle session` | Open or close the logout, reboot, and shutdown menu. |
| Toggle clipboard | `paw2-shell msg panel-toggle clipboard` | Open or close clipboard history. |
| Toggle wallpaper panel | `paw2-shell msg panel-toggle wallpaper` | Open or close wallpaper picker. |
| Toggle control center | `paw2-shell msg panel-toggle control-center [tab]` | Open or close Control Center. Optional `tab` opens the tab inside control center by default (e.g. `paw2-shell msg panel-toggle control-center media`). |

## Dock

| Action | Command | Description |
|--------|---------|-------------|
| Show dock | `paw2-shell msg dock-show` | Show all dock instances and save that override. |
| Hide dock | `paw2-shell msg dock-hide` | Hide all dock instances and save that override. |
| Toggle dock | `paw2-shell msg dock-toggle` | Toggle dock visibility and save the new state. |
| Reload dock | `paw2-shell msg dock-reload` | Reload dock configuration. |

## Desktop Widgets

| Action | Command | Description |
|--------|---------|-------------|
| Enter edit mode | `paw2-shell msg desktop-widgets-edit` | Start moving, resizing, rotating, and removing desktop widgets. |
| Exit edit mode | `paw2-shell msg desktop-widgets-exit` | Leave desktop widget edit mode. |
| Toggle edit mode | `paw2-shell msg desktop-widgets-toggle-edit` | Switch edit mode on or off. |
| Show widgets | `paw2-shell msg desktop-widgets-show` | Reveal desktop widgets now, even when the saved setting is disabled. Runtime only - does not change the saved setting, and resets on restart. |
| Hide widgets | `paw2-shell msg desktop-widgets-hide` | Hide desktop widgets now, tearing down their instances so rendering and compute stop. Runtime only - does not change the saved setting, and resets on restart. |
| Toggle widgets | `paw2-shell msg desktop-widgets-toggle` | Flip current visibility; prints `shown` or `hidden`. Runtime only - does not change the saved setting. Changing the saved Desktop > Widgets toggle cancels the override. |

## Lockscreen Widgets

| Action | Command | Description |
|--------|---------|-------------|
| Enter edit mode | `paw2-shell msg lockscreen-widgets-edit` | Edit lockscreen widget layout (session must be unlocked). |
| Exit edit mode | `paw2-shell msg lockscreen-widgets-exit` | Leave lockscreen widget edit mode. |
| Toggle edit mode | `paw2-shell msg lockscreen-widgets-toggle-edit` | Switch lockscreen widget edit mode on or off. |
