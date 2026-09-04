#!/usr/bin/env bash
# Apply the active paw2-shell theme (kitty + GTK 3/4) live from the theme JSON palette.
# Usage: apply-theme-externals.sh <slug> [kitty 0|1] [gtk 0|1]
#
# The two flags are Settings.themeKitty / Settings.themeGtk. They default to on
# so a bare manual invocation still does what it always did. A `0` does not just
# skip the write — it removes what paw put there, or the toggle would be a
# one-way door.
set -euo pipefail

slug="${1:?usage: apply-theme-externals.sh <slug> [kitty 0|1] [gtk 0|1]}"
do_kitty="${2:-1}"
do_gtk="${3:-1}"
json="$HOME/.config/quickshell/paw2-shell/themes/$slug.json"
[[ -f "$json" ]] || { echo "apply-theme-externals: no theme $slug" >&2; exit 0; }

# Load palette into BG/FG/RED/... shell vars (uppercase keys, no leading #).
# A missing or empty key falls back to fg, then to white. @sh shell-quotes each
# value so a hostile palette cannot inject through the eval below; the hex
# validator right after is the real gate on what reaches the config writers.
eval "$(jq -r '
    .palette as $p
    | ["bg","fg","red","green","yellow","blue","mauve","cyan",
       "comment","selection","orange","teal","maroon","lavender"][]
    | . as $k
    | (($p[$k] // "") | tostring) as $v
    | (if $v == "" then (($p.fg // "ffffff") | tostring) else $v end) as $v
    | "\($k | ascii_upcase)=\($v | @sh)"
' "$json")"

# ponytail: palette values are hex-only by contract. Reject anything else before
# it reaches the sed config.kdl patch (where &, / or \ would corrupt the file) or
# the kitty/gtk heredocs. Bad palette = broken theme; abort rather than write garbage.
for _c in BG FG RED GREEN YELLOW BLUE MAUVE CYAN COMMENT SELECTION ORANGE TEAL MAROON LAVENDER; do
    [[ "${!_c}" =~ ^[0-9a-fA-F]{3,8}$ ]] || {
        echo "apply-theme-externals: $slug palette $_c is not a hex color; aborting" >&2
        exit 1
    }
done

# Compositor colors are NOT applied here: paw2-shell pushes the palette straight
# to paw over IPC (services/Colors.qml -> set-theme), which repaints window
# borders, the root background, and the lock backdrop live. This script only
# handles externals paw knows nothing about.

# kitty
kdir="$HOME/.config/kitty"
kc="$kdir/current-theme.conf"
kconf="$kdir/kitty.conf"
FENCE_TOP='# --- paw2-shell theme sync (managed) ---'
FENCE_END='# --- end paw2-shell ---'

# The starter config for a machine that has none. Kept generic on purpose: the
# font is the one paw.spec already requires, remote control is left commented
# out because it lets any local process drive the terminal, and nothing here
# names an editor or a font that Fedora does not package.
seed_kitty_conf() {
cat <<EOF
# kitty configuration, seeded by paw2-shell because you had none.
# It is yours to edit — paw2-shell only ever manages the fenced block at the end,
# and will never rewrite the rest of this file.

# Cascadia Mono NF is a hard dependency of the paw package, so it is always
# present. Change it to whatever you like.
font_family      Cascadia Mono NF
bold_font        auto
italic_font      auto
bold_italic_font auto
font_size        13.0

copy_on_select yes
notify_on_cmd_finish never
scrollback_lines 2000
window_padding_width 15
allow_hyperlinks yes

# Transparency, to sit with the rest of the desktop.
background_opacity 0.85
dynamic_background_opacity yes

mouse_map left click ungrabbed mouse_handle_click selection link prompt

# Lets kittens and scripts drive this terminal. Convenient, but any process
# running as you can then read and write your session — opt in deliberately.
#allow_remote_control yes

$FENCE_TOP
include current-theme.conf
$FENCE_END
EOF
}

if [[ "$do_kitty" == 1 ]]; then

mkdir -p "$kdir"
rm -f "$kc"   # it may be a symlink → write a real file so we don't clobber the target
cat > "$kc" <<EOF
# paw2-shell theme: $slug
foreground            #$FG
background            #$BG
selection_foreground  #$BG
selection_background  #$SELECTION
cursor                #$FG
cursor_text_color     background
url_color             #$CYAN
color0  #$BG
color8  #$COMMENT
color1  #$RED
color9  #$RED
color2  #$GREEN
color10 #$GREEN
color3  #$YELLOW
color11 #$YELLOW
color4  #$BLUE
color12 #$BLUE
color5  #$MAUVE
color13 #$MAUVE
color6  #$CYAN
color14 #$CYAN
color7  #$FG
color15 #$FG
active_tab_foreground   #$BG
active_tab_background    #$FG
inactive_tab_foreground #$BG
inactive_tab_background #$COMMENT
EOF

# Nothing above matters unless kitty.conf includes it. kitty is last-value-wins,
# so the include goes at the *end*, where it beats colours already in the file;
# prepending would silently lose to them.
if [[ ! -f "$kconf" ]]; then
    seed_kitty_conf > "$kconf"
elif ! grep -qF "$FENCE_TOP" "$kconf"; then
    # An existing config is never replaced, only appended to. cp -n so a second
    # run cannot overwrite the backup with an already-fenced copy.
    cp -n "$kconf" "$kconf.pawbak" 2>/dev/null || true
    printf '\n%s\ninclude current-theme.conf\n%s\n' "$FENCE_TOP" "$FENCE_END" >> "$kconf"
fi
pkill -SIGUSR1 -x kitty 2>/dev/null || true

else

# Toggled off: drop the fence and leave everything else, including the generated
# current-theme.conf, so switching back on is instant.
# ponytail: not `sed -i` — kitty.conf is commonly a symlink (stow, chezmoi) and
# sed -i replaces the symlink with a regular file, silently breaking the link.
# Filtering to a temp and cat-ing it back writes *through* the symlink.
if [[ -f "$kconf" ]] && grep -qF "$FENCE_TOP" "$kconf"; then
    tmp=$(mktemp)
    sed "/^${FENCE_TOP}$/,/^${FENCE_END}$/d" "$kconf" > "$tmp"
    cat "$tmp" > "$kconf"
    rm -f "$tmp"
    pkill -SIGUSR1 -x kitty 2>/dev/null || true
fi

fi

# GTK 3 + 4
gtk_css() {
cat <<EOF
/* paw2-shell theme: $slug — libadwaita / adw-gtk3 named-color overrides. */
@define-color accent_color        #$BLUE;
@define-color accent_bg_color     #$BLUE;
@define-color accent_fg_color     #$BG;

@define-color window_bg_color     #$BG;
@define-color window_fg_color     #$FG;
@define-color view_bg_color       #$BG;
@define-color view_fg_color       #$FG;

@define-color headerbar_bg_color  #$SELECTION;
@define-color headerbar_fg_color  #$FG;
@define-color headerbar_border_color #$FG;
@define-color headerbar_backdrop_color #$BG;

@define-color card_bg_color       #$SELECTION;
@define-color card_fg_color       #$FG;
@define-color popover_bg_color    #$SELECTION;
@define-color popover_fg_color    #$FG;
@define-color dialog_bg_color     #$BG;
@define-color dialog_fg_color     #$FG;
@define-color sidebar_bg_color    #$BG;
@define-color sidebar_fg_color    #$FG;

@define-color destructive_bg_color #$RED;
@define-color destructive_fg_color #$BG;
@define-color destructive_color    #$RED;
@define-color success_color        #$GREEN;
@define-color success_bg_color     #$GREEN;
@define-color warning_color        #$YELLOW;
@define-color warning_bg_color     #$YELLOW;
@define-color error_color          #$RED;
@define-color error_bg_color       #$RED;

/* legacy GTK names */
@define-color theme_bg_color          #$BG;
@define-color theme_fg_color          #$FG;
@define-color theme_base_color        #$BG;
@define-color theme_text_color        #$FG;
@define-color theme_selected_bg_color #$BLUE;
@define-color theme_selected_fg_color #$BG;
@define-color insensitive_bg_color    #$BG;
@define-color insensitive_fg_color    #$COMMENT;
@define-color insensitive_base_color  #$SELECTION;
@define-color borders                 #$SELECTION;
@define-color unfocused_borders       #$SELECTION;

/* compact spacing tweaks */
headerbar, .titlebar      { min-height: 34px; padding: 0 4px; }
headerbar button          { min-height: 26px; padding: 2px 6px; margin: 2px; }
button, entry, spinbutton { min-height: 26px; padding: 2px 8px; }
row, list row             { min-height: 28px; padding: 2px 6px; }
notebook tab              { min-height: 28px; padding: 2px 8px; }
EOF
}
GTK_MARK='/* paw2-shell theme:'

mkdir -p "$HOME/.config/gtk-3.0" "$HOME/.config/gtk-4.0"
for g in "$HOME/.config/gtk-3.0/gtk.css" "$HOME/.config/gtk-4.0/gtk.css"; do
    if [[ "$do_gtk" == 1 ]]; then
        # Back up a gtk.css paw did not write, once, before overwriting it.
        if [[ -f "$g" ]] && ! grep -qF "$GTK_MARK" "$g"; then
            cp -n "$g" "$g.pawbak" 2>/dev/null || true
        fi
        gtk_css > "$g"
    elif [[ -f "$g" ]] && grep -qF "$GTK_MARK" "$g"; then
        # Remove only what we can prove we wrote, and hand back the original.
        rm -f "$g"
        if [[ -f "$g.pawbak" ]]; then
            mv "$g.pawbak" "$g"
        fi
    fi
done

# Force GTK apps to reload css by flipping the theme name to Adwaita and back.
# ponytail: this gsettings reload is a non-critical nicety — it must NOT abort the
# script. `|| true` keeps a failing gsettings probe from tripping
# `set -euo pipefail` and skipping the rest.
if command -v gsettings >/dev/null 2>&1; then
    cur=$(gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null | tr -d "'" || true)
    # The `|| true` matters: without it a false test returns 1 and set -e kills
    # the script here, before it reaches the end.
    [[ -z "$cur" || "$cur" == "Adwaita" ]] && cur="adw-gtk3-dark" || true
    gsettings set org.gnome.desktop.interface gtk-theme "Adwaita" 2>/dev/null || true
    gsettings set org.gnome.desktop.interface gtk-theme "$cur" 2>/dev/null || true
fi

echo "applied externals for $slug"
