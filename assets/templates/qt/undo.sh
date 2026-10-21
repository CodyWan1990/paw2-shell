#!/usr/bin/env bash
set -euo pipefail

config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
rm -f -- "$config_home/qt5ct/colors/paw2-shell.conf" "$config_home/qt6ct/colors/paw2-shell.conf"
