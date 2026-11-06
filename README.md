# Freccia

![Freccia playing Radiofreccia](docs/screenshot.png)

Radiofreccia on and off, from the Omarchy bar, a small window, or the
terminal. One click on the radio icon in the bar starts the station; another
stops it. The show on air and the song playing appear in the window and the
bar's tooltip.

## Install

### Omarchy and Arch Linux

Download the `freccia-*.pkg.tar.zst` from the
[latest release](https://github.com/cirolosapio/freccia/releases/latest) and
install it:

```sh
sudo pacman -U freccia-*.pkg.tar.zst
freccia bar install  # puts the radio button on the Omarchy bar
```

Or build it from this checkout:

```sh
bin/install          # builds the package and installs it with makepkg -fsi
freccia bar install
```

Freccia then shows up in the app launcher (`Super + Space`).

### Other distributions

Freccia is made for Omarchy, but only the bar button needs it. The window
and the command line are a plain Qt 6 app that runs on any Linux desktop,
Wayland or X11, with **Qt 6.5 or newer**. On other desktops the window takes
its colours from Freccia's built-in default theme, and `freccia bar install` says
there's no Omarchy bar to add the button to.

| Distribution            | Qt  | Works |
|-------------------------|-----|-------|
| Arch, Omarchy, Manjaro  | 6.8+ | yes  |
| Fedora 41+              | 6.8+ | yes  |
| Debian 13, Ubuntu 25.04+ | 6.8 | should, as long as Qt is 6.5 or newer |
| Ubuntu 24.04, Debian 12 | 6.4 or older | no, Qt is too old |

Install the build dependencies, then build and install:

```sh
# Fedora
sudo dnf install gcc-c++ make qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel

# Debian, Ubuntu
sudo apt install build-essential qmake6 qt6-base-dev qt6-declarative-dev qt6-multimedia-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  qt6-ffmpeg-multimedia-plugin
```

```sh
bin/build
sudo install -Dm755 build/freccia /usr/local/bin/freccia
sudo install -Dm644 pkgbuild/freccia.desktop /usr/local/share/applications/freccia.desktop
sudo install -Dm644 pkgbuild/freccia.svg /usr/local/share/icons/hicolor/scalable/apps/freccia.svg
```

Playing the stream needs Qt Multimedia's FFmpeg backend, which Fedora ships
with `qt6-qtmultimedia` and Debian and Ubuntu in `qt6-ffmpeg-multimedia-plugin`.

## The bar button

| Action      | Does                              |
|-------------|-----------------------------------|
| Click       | Switch the radio on or off        |
| Right-click | Open the window                   |
| Scroll      | Volume up or down                 |

The icon lights in the theme's accent while the radio plays and dims while
it connects. `freccia bar remove` takes it off again.

## Shortcuts

| Key     | Does                           |
|---------|--------------------------------|
| Space   | Switch the radio on or off     |
| ↑ ↓     | Volume                         |
| M       | Mute                           |
| Esc     | Hide the window, keep playing  |
| Q       | Quit and stop the radio        |
| ?       | Show or hide shortcuts         |

## Command line

```sh
freccia toggle | play | stop
freccia status [--json]
freccia volume 60 | +5 | -5
freccia quit
```

Each command exits 0 on success and 1 with a message on stderr otherwise.
`--json` prints the status as one JSON object.

## How it works

One `freccia` process plays the stream with Qt Multimedia. The window, the
bar, and the command line reach it through a socket in
`$XDG_RUNTIME_DIR/freccia/`, and it writes its status to `state.json` beside
it, which the bar widget watches. Closing the window leaves the radio
playing; switching the radio off with no window open ends the process.

The station address can be changed with `stream=` in
`~/.config/freccia/freccia.conf`, which also remembers the volume.

## Development

```sh
bin/build   # build/freccia
bin/test    # Qt tests offscreen, plus the bar widget's logic in Node
```

GitHub Actions runs the tests and builds the Arch package on every push.
To publish a release, bump `pkgver` in `pkgbuild/PKGBUILD` and `version` in
`plugin/manifest.json`, then push a matching tag:

```sh
git tag v0.2.0 && git push origin v0.2.0
```

The workflow checks the tag against both versions and attaches the package
to a new release.
