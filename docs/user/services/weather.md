# Weather

```toml
[weather]
enabled         = true
refresh_minutes = 30
unit            = "metric"      # metric | imperial
effects         = true          # visual weather effects
```

Weather consumes the coordinates resolved by [`[location]`](location.md) - set `auto_locate` or `address` there. When a location is available, paw2-shell fetches current weather plus a 6-day forecast. Configure under **Settings → Location → Weather**.

Weather data is provided by [Open-Meteo](https://open-meteo.com/). See the [Open-Meteo privacy policy](https://open-meteo.com/en/terms#privacy) for how requests to that service are handled.

Enabling weather adds a `Weather` tab to the control center. The bar `weather` widget type also requires weather to be enabled.
