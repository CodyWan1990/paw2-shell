# text

Fixed text on the bar. Requires a named `[widget.<name>]` entry.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `text` | string | `""` | Text shown on the bar |

```toml
[widget.hostname]
type = "text"
text = "paw2-shell"

[bar.default]
start = ["hostname", "workspaces"]
```
