# Installation

paw2-shell is the desktop shell for the [paw2](https://github.com/CodyWan1990/paw2) Wayland compositor. It isn't packaged for any distribution yet, so you build it from source.

## 1. Install the build dependencies

**Arch**

```bash
sudo pacman -S meson gcc \
  wayland wayland-protocols \
  libglvnd freetype2 fontconfig \
  cairo pango harfbuzz \
  libxkbcommon glib2 \
  libsecret libsodium \
  sdbus-cpp libpipewire wireplumber polkit \
  pam curl libwebp libjxl libsndfile librsvg \
  libqalculate libxml2 \
  md4c tomlplusplus \
  nlohmann-json stb \
  jemalloc
```
**Fedora**

```bash
sudo dnf install meson gcc-c++ \
  wayland-devel wayland-protocols-devel \
  libEGL-devel mesa-libGLES-devel \
  freetype-devel fontconfig-devel \
  cairo-devel pango-devel harfbuzz-devel \
  libxkbcommon-devel glib2-devel \
  libsecret-devel libsodium-devel \
  sdbus-cpp-devel pipewire-devel wireplumber-devel \
  pam-devel polkit-devel libcurl-devel libwebp-devel libjxl-devel libsndfile-devel librsvg2-devel \
  libqalculate-devel libxml2-devel \
  md4c-devel tomlplusplus-devel \
  json-devel stb_image_resize2-devel stb_image_write-devel \
  jemalloc-devel
```
**Debian/Ubuntu**

```bash
sudo apt install meson libwayland-dev \
  wayland-protocols libfreetype-dev libfontconfig-dev \
  libcairo2-dev libpango1.0-dev librsvg2-dev libxkbcommon-dev \
  libepoxy-dev libgles-dev libwebp-dev libcurl4-gnutls-dev \
  libmd4c-dev nlohmann-json3-dev libsdbus-c++-dev libsecret-1-dev \
  libsodium-dev libstb-dev libtomlplusplus-dev \
  libpipewire-0.3-dev libpam0g-dev libpolkit-agent-1-dev \
  libpolkit-gobject-1-dev libqalculate-dev libwireplumber-0.5-dev \
  libxml2-dev libjemalloc-dev git

```

Vendored dependencies, with no system package needed: `Wuffs`, `fzy`, and Material Color Utilities.

System packages required beyond the Wayland/GL stack: `libwebp` handles WebP decoding and thumbnail encoding. Wuffs
handles the other supported raster image formats.


Polkit agent support requires development files that provide the `polkit-agent-1` and `polkit-gobject-1` pkg-config
modules. Some distros ship these in the runtime `polkit` package, while split-package distros use names such as
`polkit-devel`, `polkit-dev`, or `libpolkit-agent-1-dev` / `libpolkit-gobject-1-dev`.

**Optional:** installing `jemalloc` (Fedora: `jemalloc-devel`, Arch: `jemalloc`, Debian/Ubuntu: `libjemalloc-dev`, Void:
`jemalloc`) reduces memory fragmentation in long-running sessions. On glibc systems it is used automatically when
detected. Use Meson's `-Djemalloc=enabled` or `-Djemalloc=disabled` option to require or disable it explicitly.

## 2. Get the source

```bash
git clone https://github.com/CodyWan1990/paw2-shell
cd paw2-shell
```

## 3. Build and install

```bash
meson setup build-release --buildtype=release -Db_lto=true
meson compile -C build-release paw2-shell
sudo meson install --no-rebuild -C build-release
```

Pass `--prefix "$HOME/.local"` to `meson setup` to install for your user only. With [just](https://github.com/casey/just) installed, `just configure release`, `just build release` and `sudo just install release` do the same.

Meson installs the binary and its assets with the normal prefix layout:

```text
/usr/local/bin/paw2-shell
/usr/local/share/paw2-shell/assets/...
```

paw2-shell needs the `assets/` tree at runtime, so copying only the binary is not enough. See [PACKAGING.md](../../../PACKAGING.md#runtime-asset-lookup) for every place it looks.

To update, pull and repeat step 3. Then continue with [Running paw2-shell](running-the-shell.md).

## Getting help

Report problems at the [issue tracker](https://github.com/CodyWan1990/paw2-shell/issues).
