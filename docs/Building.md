# Building the EasyRPG Editor

The Editor is a CMake project in C++23 with Qt 6 (>= 6.10). It needs:

- [liblcf](https://github.com/EasyRPG/liblcf), cloned and built with the
  Editor when `EDITOR_BUILD_LIBLCF` is on (the simplest), and its own needs:
  ICU, expat and inih;
- Qt 6: Base, Multimedia, SVG, Tools (Linguist, optional);
- zlib.


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

1. `brew install` what is missing among cmake, ninja, the Qt 6 modules,
   icu4c@78, inih and expat (installed packages are not upgraded);
2. configures the Editor in `build` with liblcf built alongside, and builds
   `build/EasyRPG Editor.app`; with `--tests`, also the tests, on a copy of
   TestGame-2003 downloaded into `build/testgame2003`.

Running it again only rebuilds what changed, and so do the usual commands:

```bash
cmake --build build          # rebuild
ctest --test-dir build       # the tests (configured with --tests)
```

Variables (`make` passes `BUILD_DIR` and `QT_DIR` to the script):

| Variable | Effect |
|---|---|
| `BUILD_DIR` | where the Editor is built (default `build`) |
| `JOBS` | parallel jobs (default: every core) |
| `EDITOR_TEST_PROJECT` | the game the GUI tests change (default: the downloaded copy) |
| `QT_DIR` | an official Qt (qt.io or aqt, e.g. `~/Qt/6.10.1/macos`) instead of Homebrew's, below |
| `MACOSX_DEPLOYMENT_TARGET` | with `QT_DIR`: the oldest macOS (default 13.3) |
| `ARCHS` | with `QT_DIR`: `arm64`, `x86_64` or `"arm64;x86_64"` (default: this Mac's) |
| `DEPS_DIR` | with `QT_DIR`: where ICU and inih go (default `../editor-deps`) |

If ccache is installed, the Editor is compiled through it.

### For other Macs

Homebrew builds Qt and its libraries for the macOS it runs on: an Editor built
with them runs on that version of macOS and later only. With `QT_DIR`, the
script uses nothing of Homebrew but its tools, builds ICU 78.3 and inih r62
for macOS 13.3 and later and both kinds of Mac, and the Editor for the same
macOS. CI does this.

`builds/macos/package.sh` then copies the application into `build/dist`
with what it uses (macdeployqt), signs it, and makes a disk image of it
(notarized when given Apple's credentials; see the script). It fails if a
part of the application needs a newer macOS than `MACOSX_DEPLOYMENT_TARGET`.

### By hand

The script's steps, to adapt them:

```bash
brew install cmake ninja qtbase qtmultimedia qtsvg qttools icu4c@78 inih expat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEDITOR_BUILD_LIBLCF=ON \
	-DCMAKE_PREFIX_PATH="$(brew --prefix icu4c@78);$(brew --prefix)" \
	-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"
cmake --build build
```


## Linux and Windows

Upstream's presets build zlib, inih and expat with vcpkg. Qt 6.10 (Base,
Multimedia, SVG, Tools) comes from qt.io or
[aqt](https://github.com/miurahr/aqtinstall), and on Linux ICU from the
system (`libicu-dev` on Debian and Ubuntu), as in continuous integration
(`.github/workflows/stable-compilation.yml`):

```bash
uvx --from aqtinstall aqt install-qt linux desktop 6.10.1 linux_gcc_64 -m qtmultimedia -O ~/Qt
export VCPKG_ROOT=/path/to/vcpkg
make QT_DIR=~/Qt/6.10.1/gcc_64
```

By hand: `cmake --preset vcpkg-liblcf-release -DCMAKE_PREFIX_PATH=$QT_DIR`,
then `cmake --build --preset vcpkg-liblcf-release`.

`builds/cmake/CMakePresets.json.template` lists the presets (`windows-*`,
`macos-*`, `vcpkg-*`, `liblcf-*`). The fork is mostly built and tested on macOS:
reports about other platforms are welcome.

CI uses vcpkg at the commit `VCPKG_COMMIT` of the workflow, with the
release-only triplets (`-DVCPKG_TARGET_TRIPLET=x64-linux-release`, the same
for `VCPKG_HOST_TRIPLET`), and keeps the libraries it builds and ccache's
files between runs. To move to a newer vcpkg, change `VCPKG_COMMIT`.

The workflow runs by hand (Actions, "Run workflow") or when a release is
published on GitHub: it then builds the release's tag, notarizes the macOS
application, and attaches the Flatpak, the AppImage and the disk image to the
release.


## CMake options

| Option | Default | Effect |
|---|---|---|
| `EDITOR_BUILD_LIBLCF` | `OFF` | clone liblcf into `lib/liblcf` and build it with the Editor |
| `EDITOR_BUILD_LIBLCF_GIT` | GitHub (HTTPS) | the repository cloned; an SSH URL where HTTPS is blocked |
| `EDITOR_PRECOMPILE_HEADERS` | `ON` | parse Qt Widgets and liblcf's database header once for every file (a clean build takes a quarter less time) |
| `EDITOR_TEST_PROJECT` | (none) | the game the script tests run on: a copy of TestGame-2003 |


## Tests

`make test` builds the Editor with the tests and runs them. By hand, with a
copy of [TestGame-2003](https://github.com/EasyRPG/TestGame) (the tests
change it):

```bash
cmake -B build -DEDITOR_TEST_PROJECT=/path/to/testgame2003
cmake --build build && ctest --test-dir build -j 8
```

| Test | What it checks |
|---|---|
| `script_*` | the scripts of `tests/scripts`, offscreen (see [Automation](Automation.adoc)) |


## Troubleshooting

- **ICU headers not found when building liblcf**: Homebrew's ICU is keg-only:
  pass `-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"`.
- **Cloning liblcf fails**: pass
  `-DEDITOR_BUILD_LIBLCF_GIT=git@github.com:EasyRPG/liblcf.git`, or clone it
  into `lib/liblcf` yourself.
