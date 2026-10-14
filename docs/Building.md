# Building the EasyRPG Editor

The Editor is a CMake project in C++23 with Qt 6 (>= 6.10.1; developed and
built by CI with 6.11.2). Its interface is QML (as upstream's), without Qt
Widgets. It needs:

- [liblcf](https://github.com/EasyRPG/liblcf), cloned and built with the
  Editor when `EDITOR_BUILD_LIBLCF` is on (the simplest), and its own needs:
  ICU, expat and inih;
- Qt 6: Base, Declarative (Qt Quick), Multimedia, Shader Tools, SVG, Tools
  (Linguist, optional);
- for the QML interface: KDE's [Kirigami](https://invent.kde.org/frameworks/kirigami)
  and [Breeze style](https://invent.kde.org/plasma/qqc2-breeze-style) (with
  KDE's extra-cmake-modules), [KDDockWidgets](https://github.com/KDAB/KDDockWidgets)
  (Qt Quick) and [glaze](https://github.com/stephenberry/glaze);
- zlib.

Kirigami, Breeze and KDDockWidgets use Qt's private headers: they are built
for one Qt, and built again for another.


## In short

From the repository:

| Command | Effect |
|---|---|
| `make` | build the Editor for this computer (the first time, also what it needs) |
| `make run` | open it |
| `make test` | build with the tests and run them (the test game is downloaded) |
| `make clean` | remove the build |

On macOS `make` runs `builds/macos/build.sh` (below); on Linux it uses the
vcpkg preset `vcpkg-liblcf-release` (below).


## macOS

On macOS (Apple silicon or Intel) with Xcode's command line tools
(`xcode-select --install`) and [Homebrew](https://brew.sh):

`make` runs `builds/macos/build.sh` (`make test`: with `--tests`), which:

1. `brew install` what is missing among cmake, ninja, extra-cmake-modules,
   the Qt 6 modules, icu4c@78, inih, expat and glaze (installed packages are
   not upgraded);
2. builds Kirigami, Breeze and KDDockWidgets (the versions of
   `builds/vcpkg_overlay`) for this Qt, once, into
   `../editor-deps/kde/qtVERSION-homebrew` (`-ARCHS-macosTARGET` with
   `QT_DIR`); a stamp file there names the Qt they were built for;
3. configures the Editor in `build` with liblcf built alongside, and builds
   `build/EasyRPG Editor.app`; with `--tests`, also the start-up test, on a
   copy of TestGame-2003 downloaded into `build/testgame2003`.

Running it again only rebuilds what changed, and so do the usual commands:

```bash
cmake --build build          # rebuild
ctest --test-dir build       # the start-up test (configured with --tests)
```

Variables (`make` passes `BUILD_DIR` and `QT_DIR` to the script):

| Variable | Effect |
|---|---|
| `BUILD_DIR` | where the Editor is built (default `build`) |
| `JOBS` | parallel jobs (default: every core) |
| `EDITOR_TEST_PROJECT` | the game the start-up test opens (default: the downloaded copy) |
| `QT_DIR` | an official Qt (qt.io or aqt, e.g. `~/Qt/6.11.2/macos`) instead of Homebrew's, below |
| `MACOSX_DEPLOYMENT_TARGET` | with `QT_DIR`: the oldest macOS (default 13.3) |
| `ARCHS` | with `QT_DIR`: `arm64`, `x86_64` or `"arm64;x86_64"` (default: this Mac's) |
| `DEPS_DIR` | where Kirigami, Breeze and KDDockWidgets go, and with `QT_DIR` ICU, inih and glaze (default `../editor-deps`) |

If ccache is installed, the Editor is compiled through it.

### For other Macs

Homebrew builds Qt and its libraries for the macOS it runs on: an Editor built
with them runs on that version of macOS and later only. With `QT_DIR`, the
script uses nothing of Homebrew but its tools, builds ICU 78.3 and inih r62
for macOS 13.3 and later and both kinds of Mac, and the Editor for the same
macOS. CI does this.

`builds/macos/package.sh` then copies the application into `build/dist`
with what it uses (macdeployqt, with the QML modules and the Kirigami
platform plugin; without Qt Widgets, which the Editor does not use), signs
it, and makes a disk image of it (notarized when given Apple's credentials;
see the script). It fails if a part of the application needs a newer macOS
than `MACOSX_DEPLOYMENT_TARGET`, or uses Qt Widgets.

### By hand

The script's steps, to adapt them:

```bash
brew install cmake ninja extra-cmake-modules qtbase qtdeclarative qtshadertools \
	qtmultimedia qtsvg qttools icu4c@78 inih expat glaze
# Kirigami, Breeze and KDDockWidgets built into KDE (see build.sh), then:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEDITOR_BUILD_LIBLCF=ON \
	-DCMAKE_PREFIX_PATH="$(brew --prefix icu4c@78);$(brew --prefix);$KDE" \
	-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"
cmake --build build
```


## Linux and Windows

Upstream's presets build zlib, inih, expat, glaze, Kirigami, Breeze and
KDDockWidgets with vcpkg (the KDE ones from the overlay ports of
`builds/vcpkg_overlay`, which the presets pass). Qt 6.11.2 (Base, with
Declarative, SVG and Tools; Multimedia, Shader Tools) comes from qt.io or
[aqt](https://github.com/miurahr/aqtinstall), and on Linux ICU from the
system (`libicu-dev` on Debian and Ubuntu), as in continuous integration
(`.github/workflows/stable-compilation.yml`):

```bash
uvx --from aqtinstall aqt install-qt linux desktop 6.11.2 linux_gcc_64 -m qtmultimedia qtshadertools -O ~/Qt
export VCPKG_ROOT=/path/to/vcpkg
make QT_DIR=~/Qt/6.11.2/gcc_64
```

(`linux_arm64` and `linux_gcc_arm64` on ARM, giving `~/Qt/6.11.2/gcc_arm64`.)
By hand: `export Qt6_Path=$QT_DIR` (the KDE ports of the overlay build
against it), `cmake --preset vcpkg-liblcf-release -DCMAKE_PREFIX_PATH=$QT_DIR`,
then `cmake --build --preset vcpkg-liblcf-release`. vcpkg does not notice a
change of Qt: after one, remove `build/vcpkg-liblcf-release/vcpkg_installed`
and vcpkg's binary cache of these ports.

There Kirigami and Breeze are static libraries, linked into the Editor. Their
colours (`Kirigami.Theme`) follow the system palette, light or dark, as on
macOS: they come from Kirigami's own `styles/org.kde.desktop/Theme.qml`.
Breeze's Kirigami platform plugin, which would take them from KDE's colour
schemes (KColorScheme, KConfig, KIconThemes, KGuiAddons), is left out by
`builds/vcpkg_overlay/qqc2-breeze-style/no-plasma-integration.patch`, on
macOS too (build.sh applies it); the Flatpak takes Kirigami and Breeze from
KDE's runtime instead.
`KIRIGAMI_FORCE_STYLE=1` makes Kirigami look for a `Theme.qml` of Breeze
only, which has none: fixed light colours, and a warning about `Theme.qml`.

`builds/cmake/CMakePresets.json.template` lists the presets (`windows-*`,
`macos-*`, `vcpkg-*`, `liblcf-*`). The fork is mostly built and tested on macOS:
reports about other platforms are welcome.

CI uses vcpkg at the commit `VCPKG_COMMIT` of the workflow, with the
release-only triplets (`-DVCPKG_TARGET_TRIPLET=x64-linux-release`, the same
for `VCPKG_HOST_TRIPLET`; `x64-windows-static-md-release` on Windows), and keeps the libraries it builds (per Qt
version) and ccache's files between runs. To move to a newer vcpkg, change
`VCPKG_COMMIT`; to another Qt, `QT_VERSION` (and the KDE runtime of the
Flatpak, `builds/flatpak/org.easyrpg.editor.yml` and its container image, to
a KDE runtime with that Qt).

The packages: the Flatpak, two AppImages (x86_64 and aarch64, each built on
a runner of its kind), the macOS disk image (both kinds of Mac) and the
Windows zip. The start-up test (below) runs on what each job built,
offscreen; the AppImages, the macOS application and the Windows zip are also
started on the test game from the package itself, and fail if they carry Qt
Widgets.

The workflow runs by hand only (Actions, "Run workflow"), from `editor-ux`:
a run reads the caches of its own branch and of the default branch, and
those of a tag would start empty. To release: publish the release on GitHub
(tag `v` and the version of `CMakeLists.txt`), then run the workflow with
"Attach to the latest release". It notarizes the macOS application, and
each job attaches its package to that release as soon as it has it, whatever
the other jobs do, replacing a file of the same name. It checks first: it builds nothing when the latest release has another
version.


## CMake options

| Option | Default | Effect |
|---|---|---|
| `EDITOR_BUILD_LIBLCF` | `OFF` | clone liblcf into `lib/liblcf` and build it with the Editor |
| `EDITOR_BUILD_LIBLCF_GIT` | GitHub (HTTPS) | the repository cloned; an SSH URL where HTTPS is blocked |
| `EDITOR_PRECOMPILE_HEADERS` | `ON` | parse Qt Quick and liblcf's database header once for every file (a clean build takes a quarter less time); left off when ccache is used |
| `EDITOR_TEST_PROJECT` | (none) | the game the start-up test opens: a copy of TestGame-2003 |


## Test

One test, `startup`: the Editor starts offscreen on a copy of
[TestGame-2003](https://github.com/EasyRPG/TestGame), shows its map tree and
quits (`tests/startup.txt`). `make test` builds the Editor and runs it, on
macOS and Linux; the first time, `builds/testgame.sh` downloads the game. By
hand:

```bash
cmake -B build -DEDITOR_TEST_PROJECT=/path/to/testgame2003
cmake --build build && ctest --test-dir build
```


## Troubleshooting

- **ICU headers not found when building liblcf**: Homebrew's ICU is keg-only:
  pass `-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"`.
- **Cloning liblcf fails**: pass
  `-DEDITOR_BUILD_LIBLCF_GIT=git@github.com:EasyRPG/liblcf.git`, or clone it
  into `lib/liblcf` yourself.
