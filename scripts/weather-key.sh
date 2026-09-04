#!/usr/bin/env bash
# Read/write the OpenWeatherMap API key in weather.conf (gitignored).
# Usage: weather-key.sh get | set <key> | --self-test
# ponytail: weather.conf is the ONLY place the key lives; settings.json never holds it.
set -euo pipefail

conf="${WEATHER_CONF:-$HOME/.config/quickshell/paw2-shell/weather.conf}"

get() {
    [[ -f "$conf" ]] || { echo ""; return; }
    # shellcheck disable=SC1090
    ( set +u; source "$conf" 2>/dev/null; printf '%s' "${OWM_API_KEY:-}" )
}

set_key() {
    local key="${1:-}"
    # Reject anything that isn't a plausible token (blocks shell-meta / whitespace).
    [[ "$key" =~ ^[A-Za-z0-9._-]*$ ]] || { echo "weather-key: invalid key" >&2; exit 1; }
    mkdir -p "$(dirname "$conf")"
    local line="OWM_API_KEY=\"$key\""
    if [[ -f "$conf" ]] && grep -q '^OWM_API_KEY=' "$conf"; then
        # Replace just the key line, preserve any other OWM_* vars.
        sed -i "s|^OWM_API_KEY=.*|$line|" "$conf"
    else
        printf '%s\n' "$line" >> "$conf"
    fi
    chmod 600 "$conf"
}

case "${1:-}" in
    get) get ;;
    set) set_key "${2:-}" ;;
    --self-test)
        tmp="$(mktemp -d)"; export WEATHER_CONF="$tmp/weather.conf"
        "$0" set TESTkey_123.4
        [[ "$("$0" get)" == "TESTkey_123.4" ]] || { echo "FAIL: set/get roundtrip"; exit 1; }
        [[ "$(stat -c '%a' "$WEATHER_CONF")" == "600" ]] || { echo "FAIL: perms not 600"; exit 1; }
        printf 'OWM_CITY="Keep"\n' >> "$WEATHER_CONF"   # add an unrelated var
        "$0" set NEWkey
        grep -q '^OWM_CITY="Keep"$' "$WEATHER_CONF" || { echo "FAIL: clobbered other vars"; exit 1; }
        [[ "$("$0" get)" == "NEWkey" ]] || { echo "FAIL: update did not stick"; exit 1; }
        "$0" set ""; [[ -z "$("$0" get)" ]] || { echo "FAIL: clear"; exit 1; }
        rm -rf "$tmp"; echo "weather-key self-test: PASS" ;;
    *) echo "usage: weather-key.sh get | set <key> | --self-test" >&2; exit 2 ;;
esac
