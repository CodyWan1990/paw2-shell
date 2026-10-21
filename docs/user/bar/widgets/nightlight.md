# nightlight

| State | Icon | Meaning |
|-------|------|---------|
| Off | moon-off | Night light disabled |
| On | moon | Scheduled - follows `[location]` (`sunset`/`sunrise` or coordinates) |
| Forced | moon-stars | Always-on override - ignores schedule |

Click behavior:

- **Left-click**: toggles night light on/off. If currently *forced*, this drops the force override and lands on the regular scheduled-on state, so force is reversible without also turning night light off.
- **Right-click**: toggles the always-on (force) override.

User toggles made via the bar widget (or control center) are preserved across config hot-reload - only an actual change to `enabled` / `force` in your config file resets the corresponding override.

No configurable settings.

**Command-line controls:**

```sh
paw2-shell msg nightlight-toggle
paw2-shell msg nightlight-force-toggle
```
