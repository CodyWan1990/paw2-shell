# Packaging paw2-shell

Notes for distribution packagers. End-user install docs live in the
[README](README.md) and [docs/user/getting-started/installation.md](docs/user/getting-started/installation.md).

## Package description

Use this exact short description for package metadata (`pkgdesc`, `Summary`,
AppStream, etc.):

> Desktop shell for the paw2 Wayland compositor.

Do not substitute shorter or alternate blurbs (“lightweight Wayland bar”,
“status bar”, ...). paw2-shell is a full desktop shell (bars, dock, launcher,
notifications, lock screen, wallpaper, settings, ...), not a bar-only tool.

## Identity

| | |
|---|---|
| Name | `paw2-shell` |
| Homepage | https://github.com/CodyWan1990/paw2-shell |
| Docs | [`docs/user`](docs/user/index.md) |
| License | MIT ([LICENSE](LICENSE)); also see vendored licenses under `third_party/` for SPDX completeness |
| Version | [`VERSION`](VERSION), read by Meson `project(... version: files('VERSION'))` |
| Binary | `paw2-shell` |
| Desktop entry | `paw2-shell.desktop` |
| Icon | `paw2-shell` (`share/icons/hicolor/scalable/apps/paw2-shell.svg`) |

## Maintenance Policy

Releases strive to follow [semantic versioning](https://semver.org).
The latest version is the only actively maintained version.
No attempts are made to achieve overlapping lifecycles of major or minor versions.
Bug reports are required to be reproducible on the latest version.

## Architectures / libc

First-class targets:

- `x86_64` (amd64)
- `aarch64` (arm64)

Linux only. There is no intentional arch-specific code path; other little-endian
Linux arches may build if dependencies are available, but they are not
first-class supported.

**musl**: known to build (e.g. Alpine / Void musl). Not first-class tested.
`jemalloc` is glibc-only, on musl leave `-Djemalloc=disabled` (or `auto`, which
skips it).

## Compilers / linkers

C++23 is required.

| Toolchain | Minimum | Notes |
|---|---|---|
| GCC + libstdc++ | **GCC 13+** | Default on most distros |
| Clang + libstdc++ or libc++ | **Clang 16+** | libc++ needs Meson’s experimental-library flags (already wired in `meson.build`) |

Debian 12 “bookworm” needs a newer toolchain than the default `g++` (e.g.
`g++-13` with `CXX=g++-13`).

No special linker is required. The default linker from the compiler driver is
fine (**GNU ld** / gold, **lld**, **mold** via `-fuse-ld=` all work). Release
builds pass `-Wl,--gc-sections` and `-Wl,--as-needed`, which those linkers
support.

## Build

- Build system: [Meson](https://mesonbuild.com/) + Ninja. The repo’s
  [`justfile`](justfile) is convenience only; packaging can call Meson
  directly.
- Language / toolchain: see [Compilers / linkers](#compilers--linkers).
- Recommended: `meson setup build --buildtype=release`, then compile/install.
- Do **not** enable `-Dnative_optimizations=true` for distro packages (CPU-local
  codegen; not portable).
- Tests: `-Dtests=disabled` (or leave `auto`, which skips tests for release).
- `jemalloc`: recommended on glibc; Meson feature option `-Djemalloc=auto|enabled|disabled`.
  Only used on glibc builds.

Prefix/datadir are baked into the binary via `PAW2_SHELL_INSTALL_PREFIX` /
`PAW2_SHELL_INSTALL_DATADIR`. Install with the same prefix you configured.

### Installed layout

```text
<prefix>/bin/paw2-shell
<prefix>/share/paw2-shell/assets/...
<prefix>/share/applications/paw2-shell.desktop
<prefix>/share/icons/hicolor/scalable/apps/paw2-shell.svg
```

The shipped `assets/` tree is **required at runtime**. Shipping only the binary
breaks fonts, translations, templates, glyphs, and sounds.

### Runtime asset lookup

Besides the prefix layout above, portable bundle layouts are supported:

```text
bundle/
  paw2-shell
  assets/
```

```text
bundle/
  bin/paw2-shell
  share/paw2-shell/assets/
```

paw2-shell resolves the asset root in this order:

1. `PAW2_SHELL_ASSETS_DIR` (a warning is logged if it is set but invalid)
2. `assets/` next to the executable
3. `assets/` one level above the executable (this is how a `build-debug/`
   binary finds the source tree's `assets/`)
4. `../<datadir>/paw2-shell/assets` relative to the executable, when the
   configured Meson `datadir` is relative
5. `../share/paw2-shell/assets` relative to the executable
6. the compiled install path from Meson (`<prefix>/<datadir>/paw2-shell/assets`)

A candidate is only accepted if it contains `emoji.json`,
`fonts/paw2-tabler.ttf`, `templates/builtin.toml`, and
`translations/en.json`. If none qualifies, paw2-shell logs a warning and uses the
compiled install path.

Not shipped (don’t look for them in the install): AppStream / metainfo XML, man
pages, systemd units.

### Shell completions

Static completion files are not shipped. Generate them from the freshly built
binary and install them in the distribution's completion directories:

```text
./build-<mode>/paw2-shell completions bash  → share/bash-completion/completions/paw2-shell
./build-<mode>/paw2-shell completions zsh   → share/zsh/site-functions/_paw2_shell
./build-<mode>/paw2-shell completions fish  → share/fish/vendor_completions.d/paw2-shell.fish
```

This is intentionally a packaging step rather than a Meson install rule:
executing the target binary during installation would break cross-compilation.

## Dependencies

No Qt or GTK. UI is Wayland + OpenGL ES (EGL/GLES, or `libepoxy` as a fallback
when separate EGL/GLES packages are missing).

### Build-time (required)

Canonical list is the `dependency(...)` / header checks in
[`meson.build`](meson.build). Distro package names differ; [BUILDING.md](BUILDING.md)
has copy-paste install lines for common distros.

Notes packagers hit often:

- **stb** must provide `stb/stb_image_resize2.h` (and `stb/stb_image_write.h`).
  Older packages that only ship `stb_image_resize` are not enough. Meson fails
  the configure check if `stb_image_resize2` is missing.
- Meson requires **WirePlumber 0.5** (`wireplumber-0.5` pkg-config). 0.4 is not
  enough.

### Vendored (no system package)

Shipped under `third_party/`: Wuffs, fzy, Material Color Utilities.
Each carries its own license file beside the code.

### Runtime

| Dependency | Role |
|---|---|
| PipeWire **daemon** (+ WirePlumber 0.5) | Audio, volume OSD, privacy indicators, spectrum. Libraries alone are not enough; without a running daemon those features stay off. |
| PAM (`login` service by default) | Lock screen authentication |
| Fontconfig / fonts | Text rendering (users still need usable fonts installed) |
| `upower` | Optional: battery / power devices |
| `ddcutil` | Optional: external monitor brightness |
| Secret Service provider | Optional but recommended for credential / encrypted-state persistence (GNOME Keyring, KWallet, KeePassXC, ...). `libsecret` is only the client library; without a session provider those features cannot persist secrets. |
| `sound-theme-freedesktop` | Shell sounds |

## Startup and IPC

No systemd user unit is shipped. Typical startup is paw2's `[autostart]` or the
desktop entry (`paw2-shell --daemon`). Control a running instance with
`paw2-shell msg ...` (Unix socket under `XDG_RUNTIME_DIR`).

Keep the `.desktop` daemon Exec unless you intentionally package a different
session integration. Prefer the canonical description above for `Comment=` /
AppStream summary as well.

## Session conflicts

paw2-shell provides and registers:

- `org.freedesktop.Notifications`
- `org.kde.StatusNotifierWatcher` (system tray host)

Do not run it alongside another notification daemon or StatusNotifier host
(mako, dunst, swaync, waybar-as-host, ...) unless those are disabled.

## User data paths

| Kind | Default |
|---|---|
| Config | `$XDG_CONFIG_HOME/paw2-shell` (`~/.config/paw2-shell`), e.g. `config.toml` |
| State | `$XDG_STATE_HOME/paw2-shell` (`~/.local/state/paw2-shell`), e.g. `settings.toml`, caches |
| Data | `$XDG_DATA_HOME/paw2-shell` (`~/.local/share/paw2-shell`) |
| Logs | `$XDG_CACHE_HOME/paw2-shell` (`~/.cache/paw2-shell`) |

Override bases with `PAW2_SHELL_CONFIG_HOME`, `PAW2_SHELL_STATE_HOME`,
`PAW2_SHELL_DATA_HOME` (each still appends `/paw2-shell`).

## What paw2-shell is not

- Not a compositor, display manager, or greeter. It runs only on
  [paw2](https://github.com/CodyWan1990/paw2).
- Not a replacement for file managers, screen casting, or drive mounting.

## Versioning

Prefer packaging **tagged releases** rather than random `main` snapshots unless
you maintain a `-git` / nightly package on purpose.

## Contact

- Issues: https://github.com/CodyWan1990/paw2-shell/issues
