# notifications

Shows the pending notification count. Right-clicking toggles transient Do Not Disturb (DND) mode.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `hide_when_no_unread` | bool | `false` | Hide the widget when there are no unread notifications |

```toml
[widget.notifications]
hide_when_no_unread = true
```

**Command-line controls:**

```sh
paw2-shell msg notification-dnd-toggle
paw2-shell msg notification-dnd-status
paw2-shell msg notification-clear-active
paw2-shell msg notification-clear-history
```

Enabling DND immediately hides visible toast popups and suppresses new external notification toasts. Hidden and
suppressed notifications remain in history/control center. Disabling DND allows new toast popups but does not replay
the hidden or suppressed popups. Internal paw2-shell notifications are not stored in history.
