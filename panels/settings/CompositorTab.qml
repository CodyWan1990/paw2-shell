import QtQuick
import "../../services" as Services
import "../../components" as Components

/// Compositor settings — paw2's own runtime config.
///
/// The rows are driven by a spec table rather than written out one by one:
/// adding a setting should be one line, not a new block of QML. Values come
/// from the compositor (Services.PawConfig), never from defaults duplicated
/// here — the `fallback` on each row only covers the moment before paw2's first
/// config message arrives.
Components.SettingsPage {
    id: root
    anchors.fill: parent

    // Writes are debounced; leaving the tab within that window would otherwise
    // drop the last edit. No-op when nothing is pending.
    Component.onDestruction: Services.PawConfig.flush()

    readonly property var spec: [
        { section: "Tiling" },
        // Bounds mirror paw2's validation in config.rs, so anything a slider
        // can reach is a value paw2 will accept. `maxPath` covers the one rule
        // that couples two settings.
        { path: "layout.column_width", label: "Column width", type: "float",
          from: 0.1, to: 1.0, step: 0.05, decimals: 2, fallback: 0.6 },
        { path: "layout.center_focused_column", label: "Centre the focused column",
          type: "bool", fallback: false },
        // [layout] is written as a whole table, so it needs a control for every
        // key in it or the ones without would freeze at whatever they held when
        // the shell first claimed the section.
        { path: "layout.enabled_layouts", label: "Layouts", type: "csv",
          fallback: [] },
        { type: "note", text: "Layouts is the list Cycle layout steps through, "
          + "in order: scrolling, dwindle, master_stack, monocle." },

        { section: "Appearance" },
        { path: "appearance.gaps", label: "Window gaps", type: "int",
          from: 0, to: 256, suffix: "px", fallback: 15 },
        // paw2 rejects a ring wider than the gap it has to fit in, and the shell
        // now writes both sides of that, so it is the shell's rule to keep.
        { path: "appearance.focus_ring_width", label: "Focus ring width", type: "int",
          from: 0, to: 256, maxPath: "appearance.gaps", suffix: "px", fallback: 1 },
        { path: "appearance.focus_ring_color", label: "Focus ring colour", type: "text",
          fallback: "#0b3d91" },
        { path: "appearance.corner_radius", label: "Corner radius", type: "int",
          from: 0, to: 64, suffix: "px", fallback: 8 },

        { section: "Shadows" },
        // Either a zero radius or a zero opacity turns shadows off; there is no
        // separate switch, so neither row needs a companion toggle.
        { path: "appearance.shadow_radius", label: "Shadow radius", type: "int",
          from: 0, to: 64, suffix: "px", fallback: 12 },
        { path: "appearance.shadow_opacity", label: "Shadow opacity", type: "float",
          from: 0.0, to: 1.0, step: 0.05, decimals: 2, fallback: 0.45 },
        { path: "appearance.shadow_color", label: "Shadow colour", type: "text",
          fallback: "#000000" },
        { path: "appearance.shadow_offset_x", label: "Shadow offset X", type: "int",
          from: -64, to: 64, suffix: "px", fallback: 0 },
        { path: "appearance.shadow_offset_y", label: "Shadow offset Y", type: "int",
          from: -64, to: 64, suffix: "px", fallback: 4 },
        { type: "note", text: "The shadow reaches Shadow radius past the window "
          + "on every side, so it needs Window gaps to have room to show in." },

        { section: "Transparency & blur" },
        { path: "appearance.transparency", label: "Window transparency", type: "bool",
          fallback: true },
        { path: "appearance.opacity", label: "Window opacity", type: "float",
          from: 0.1, to: 1.0, step: 0.05, decimals: 2, fallback: 0.95 },
        { path: "appearance.blur", label: "Backdrop blur", type: "bool", fallback: true },
        { path: "appearance.blur_radius", label: "Blur radius", type: "int",
          from: 0, to: 64, fallback: 8 },

        { section: "Animations" },
        { path: "animations.enabled", label: "Animations", type: "bool", fallback: true },
        { path: "animations.duration_ms", label: "Duration", type: "int",
          from: 0, to: 5000, suffix: "ms", fallback: 150 },
        // For watching an animation in slow motion; 3 runs it three times slower.
        { path: "animations.slowdown", label: "Slow motion", type: "float",
          from: 0.1, to: 10.0, step: 0.1, decimals: 1, fallback: 1.0 },

        { section: "Input" },
        { path: "input.focus_follows_mouse", label: "Focus follows mouse", type: "bool",
          fallback: false },
        { path: "input.middle_click_paste", label: "Middle-click paste (restart)",
          type: "bool", fallback: true },

        // No [backend] row on purpose. A drop-in owns every table it names, and
        // that table also holds `drm_device` and `connector` - the two settings
        // someone hand-edits to recover a machine that will not start. Owning
        // them to expose one gaming toggle is not a trade worth making, so
        // `direct_scanout` stays in config.toml.

        { section: "Desktop theme" },
        // Cursor, GTK and icon settings use their native live paths. Qt style
        // and font are applied to child processes at startup.
        //
        // `source` names a list in Services.DesktopThemes, resolved per row in
        // the delegate below. `preset` rather than `choice` so every one keeps a
        // Custom… escape: a theme the scan misses is still typeable.
        { path: "theme.cursor_theme", label: "Cursor theme", type: "preset",
          source: "cursors", fallback: "", hint: "A directory under /usr/share/icons or ~/.icons." },
        { path: "theme.cursor_size", label: "Cursor size", type: "int",
          from: 1, to: 256, suffix: "px", fallback: 24 },
        { path: "theme.icon_theme", label: "Icon theme", type: "preset",
          source: "icons", fallback: "", hint: "A directory under /usr/share/icons or ~/.icons." },
        { path: "theme.gtk_theme", label: "GTK theme", type: "preset",
          source: "gtk", fallback: "", hint: "A directory under /usr/share/themes or ~/.themes." },
        // Shown only while the palette cannot actually reach GTK3 - see warning().
        { type: "note", warnIf: "gtkPalette",
          text: "⚠ Theme colours are written to gtk.css as adw-gtk3 / libadwaita "
              + "colour names. The GTK theme above does not read them, so your "
              + "palette will not reach GTK3 apps. Pick an adw-gtk3 theme, or turn "
              + "the GTK toggle off on the Themes tab. GTK4 apps are themed either way." },
        { path: "theme.qt_style", label: "Qt style (restart)", type: "preset",
          source: "qtstyles", fallback: "", hint: "An installed Qt style plugin." },
        // Free text on purpose: this is a Pango description - family *and* size,
        // e.g. "Monofur Nerd Font Propo 12" - so a family list would drop the size.
        { path: "theme.font", label: "Interface font (restart)", type: "text", fallback: "" },
        { type: "note", text: "Interface font is a family followed by a size, "
              + "for example \"Monofur Nerd Font Propo 12\"." }
    ]

    /// Live option lists for `source` rows. Named rather than inlined so the
    /// spec table stays a table.
    function catalogue(source) {
        if (source === "gtk")      return Services.DesktopThemes.gtkThemes;
        if (source === "icons")    return Services.DesktopThemes.iconThemes;
        if (source === "cursors")  return Services.DesktopThemes.cursorThemes;
        if (source === "qtstyles") return Services.DesktopThemes.qtStyles;
        return [];
    }

    /// Conditions for `warnIf` rows.
    ///
    /// gtkPalette: the theme externals script writes the palette as adw-gtk3 /
    /// libadwaita colour names, which only an adw-gtk3 theme reads. With any
    /// other GTK theme the colours are written and silently ignored, which is
    /// the kind of thing you would otherwise spend an evening on.
    function warning(kind) {
        if (kind !== "gtkPalette")
            return false;
        const gtk = Services.PawConfig.get("theme.gtk_theme", "");
        return Services.Settings.themeGtk && gtk !== "" && gtk.indexOf("adw-gtk3") !== 0;
    }

    Components.SectionHeader {
        x: root.horizontalPadding; width: root.rowWidth
        text: "Compositor"; showDivider: false
    }

    Components.StyledText {
        x: root.horizontalPadding
        width: root.rowWidth
        text: "These are paw2's own settings. Changes apply within about a second "
            + "— no restart, no rebuild — except where a label says otherwise.<br>"
            + "Your <tt>" + Services.Paths.pawDir + "/config.toml</tt> is never "
            + "written by the shell. Edits here go to <tt>config.d/shell.toml</tt> "
            + "beside it, which paw2 merges over your config, so hand-edits keep "
            + "their comments and ordering. Delete that file to drop every change "
            + "made here at once."
        textFormat: Text.RichText
        color: Services.Colors.color8
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Components.StyledText {
        x: root.horizontalPadding
        width: root.rowWidth
        visible: !Services.PawConfig.ready
        text: "⚠ Not connected to paw2 yet — no values to show."
        color: Services.Colors.yellow
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Column {
        x: root.horizontalPadding
        width: root.rowWidth
        spacing: 2
        visible: !Services.PawConfig.ok && Services.PawConfig.errors.length > 0

        Components.StyledText {
            text: "⚠ paw2 rejected the config and kept the previous one:"
            color: Services.Colors.red
            font.pixelSize: 12
        }
        Repeater {
            model: Services.PawConfig.errors
            delegate: Text {
                required property var modelData
                text: "   " + modelData
                color: Services.Colors.color8
                font.family: "monospace"
                font.pixelSize: 11
            }
        }
    }

    Repeater {
        model: root.spec

        delegate: Components.SettingsOptionEditor {
            required property var modelData
            x: root.horizontalPadding
            width: root.rowWidth
            // Resolved here rather than in `spec`, which is one binding for the
            // whole table: a live value in there would rebuild every row - and
            // the slider being dragged - each time the bound moved.
            spec: modelData.maxPath
                ? Object.assign({}, modelData,
                    { to: Services.PawConfig.get(modelData.maxPath, modelData.to) })
                : modelData
            // Keyed off `path`, not `section`: a `note` row has neither, and
            // asking PawConfig for the value of a row that has no path threw
            // on every rebuild of the page.
            value: modelData.path
                ? Services.PawConfig.get(modelData.path, modelData.fallback ?? [])
                : undefined
            choices: modelData.source
                ? root.catalogue(modelData.source) : (modelData.choices ?? [])
            labelWidth: 160
            visible: (Services.PawConfig.ready || !!modelData.section)
                && (!modelData.warnIf || root.warning(modelData.warnIf))
            onValueEdited: value => {
                Services.PawConfig.set(modelData.path, value);
                // `maxPath` stops the ring going above the gap; this is the
                // other direction, where the gap comes down to meet the ring.
                if (modelData.path === "appearance.gaps"
                    && Services.PawConfig.get("appearance.focus_ring_width", 1) > value)
                    Services.PawConfig.set("appearance.focus_ring_width", value);
            }
        }
    }
}
