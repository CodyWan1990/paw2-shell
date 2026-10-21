# Running paw2-shell

### Start it with paw2

Add paw2-shell to the `[autostart]` table in `~/.config/paw2/config.toml`:

```toml
[autostart]
commands = [["paw2-shell", "--daemon"]]
```

`--daemon` returns once the shell has initialized. The optional polkit authentication agent starts in the background after the UI is up.

paw2 reads `[autostart]` when it starts, so log out and back in after adding it.

### Run it by hand

To test or debug, run it from a terminal inside a paw2 session:

```bash
paw2-shell
```

Only one instance runs per display. Stop the running one first with `pkill -x paw2-shell`.

### Logs

paw2-shell writes its log to `$XDG_CACHE_HOME/paw2-shell/paw2-shell.log`, or `~/.cache/paw2-shell/paw2-shell.log` when `XDG_CACHE_HOME` is unset. When the log reaches 1 MiB, the previous one is kept as `paw2-shell.log.1`.
