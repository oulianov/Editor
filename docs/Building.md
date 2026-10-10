# Building the EasyRPG Editor

The Editor is a CMake project in C++17 with Qt 6 (>= 6.10). It needs:

- [liblcf](https://github.com/EasyRPG/liblcf), cloned and built with the
  Editor when `EDITOR_BUILD_LIBLCF` is on (the simplest), and its own needs:
  ICU, expat and inih;
- Qt 6: Base, Declarative, Multimedia, SVG, Shader Tools, Tools (Linguist,
  optional);
- KDE's Extra CMake Modules, Kirigami and qqc2-breeze-style;
- KDAB's KDDockWidgets (Qt Quick front end) and KDBindings;
- glaze and zlib.


## In short

From the repository:

| Command | Effect |
|---|---|
| `make` | build the Editor for this computer, its dependencies included |
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
   extra-cmake-modules, glaze, icu4c@78, inih and expat (installed packages
   are not upgraded);
2. downloads and builds the four libraries Homebrew does not have, once, into
   `../editor-deps` (next to the repository):

   | Library | Version | Options |
   |---|---|---|
   | KDBindings | 1.1.0 | |
   | KDDockWidgets | 2.4.0 | `-DKDDockWidgets_QT6=ON -DKDDockWidgets_FRONTENDS=qtquick` |
   | Kirigami | 6.23.0 | `-DBUILD_EXAMPLES=OFF` |
   | qqc2-breeze-style | 6.6.1 | with `builds/vcpkg_overlay/qqc2-breeze-style/no-plasma-integration.patch` |

3. configures the Editor in `build` with liblcf built alongside, and builds
   `build/EasyRPG Editor.app`; with `--tests`, also the script runner and the
   tests, on a copy of TestGame-2003 downloaded into `build/testgame2003`.

Running it again only rebuilds what changed. Variables:

| Variable | Effect |
|---|---|
| `DEPS_DIR` | where the four libraries go (default `../editor-deps`) |
| `BUILD_DIR` | where the Editor is built (default `build`) |
| `JOBS` | parallel jobs (default: every core) |
| `EDITOR_TEST_PROJECT` | the game the GUI tests change (default: the downloaded copy) |

After the first run, the usual commands work as well:

```bash
cmake --build build          # rebuild
ctest --test-dir build       # the tests (configured with --tests)
```

### By hand

The script's steps, to adapt them:

```bash
brew install cmake ninja qtbase qtdeclarative qtmultimedia qtsvg qttools qtshadertools \
	extra-cmake-modules glaze icu4c@78 inih expat
# KDBindings, KDDockWidgets, Kirigami and qqc2-breeze-style: cmake, build and
# install each into $DEPS (see the table above)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEDITOR_BUILD_LIBLCF=ON \
	-DCMAKE_PREFIX_PATH="$DEPS;$(brew --prefix icu4c@78);$(brew --prefix)" \
	-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"
cmake --build build
```


## Linux and Windows

Upstream's presets build every dependency with vcpkg, as its continuous
integration does (`.github/workflows/stable-compilation.yml`):

```bash
export VCPKG_ROOT=/path/to/vcpkg
make            # or: cmake --preset vcpkg-liblcf-release && cmake --build --preset vcpkg-liblcf-release
```

`builds/cmake/CMakePresets.json.template` lists the presets (`windows-*`,
`macos-*`, `vcpkg-*`, `liblcf-*`); `builds/vcpkg_overlay` holds the recipes of
the libraries vcpkg lacks. The fork is mostly built and tested on macOS:
reports about other platforms are welcome.


## CMake options

| Option | Default | Effect |
|---|---|---|
| `EDITOR_BUILD_LIBLCF` | `OFF` | clone liblcf into `lib/liblcf` and build it with the Editor |
| `EDITOR_BUILD_LIBLCF_GIT` | GitHub (HTTPS) | the repository cloned; an SSH URL where HTTPS is blocked |
| `EDITOR_SCRIPTS` | `OFF` | build the script runner (`--script FILE`) and the tests |
| `EDITOR_TEST_PROJECT` | (none) | the game the GUI tests run on: a copy of TestGame-2003 |


## Tests

`make test` builds with `EDITOR_SCRIPTS=ON` and runs them. By hand, with a
copy of [TestGame-2003](https://github.com/EasyRPG/TestGame) (the tests
change it):

```bash
cmake -B build -DEDITOR_SCRIPTS=ON -DEDITOR_TEST_PROJECT=/path/to/testgame2003
cmake --build build && ctest --test-dir build -j 8
```

| Test | What it checks |
|---|---|
| `data_merge` | the three-way merges of the database, maps and map tree |
| `merge_with_other_program` | the Editor and another program changing the game at once |
| `view_state_restart` | what the Editor remembers between sessions |
| `script_*` | the scripts of `tests/scripts`, offscreen (see [Automation](Automation.adoc)) |
| `native_*` | the scripts of `tests/scripts/native`, in the real style of the system: their windows show on screen |


## Troubleshooting

- **`Could not find ... KF6Kirigami`** (or KDDockWidgets-qt6, QQC2BreezeStyle):
  the four libraries are not in `CMAKE_PREFIX_PATH`. Run
  `builds/macos/build.sh`, or add their install folder to the path.
- **ICU headers not found when building liblcf**: Homebrew's ICU is keg-only:
  pass `-DICU_INCLUDE_DIR="$(brew --prefix icu4c@78)/include"`.
- **Cloning liblcf fails**: pass
  `-DEDITOR_BUILD_LIBLCF_GIT=git@github.com:EasyRPG/liblcf.git`, or clone it
  into `lib/liblcf` yourself.
