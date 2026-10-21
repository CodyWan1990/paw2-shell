# settings

Shows a glyph/image and opens the Settings window on click.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `glyph` | string | `"settings"` | Bar glyph name: paw2-shell alias, Tabler icon name, or `U+` / `0x` codepoint |
| `custom_image` | string | `""` | Path to a custom image displayed instead of the glyph. Leave empty to use the glyph |
| `custom_image_colorize` | bool | `false` | When `true`, tint the custom image with the widget icon color |

```toml
[widget.settings]
custom_image = "/path/to/settings.png"
custom_image_colorize = true
```
