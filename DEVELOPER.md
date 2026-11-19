# Developing the EasyRPG Editor (oulianov fork)

## Build from source

```bash
git clone git@github.com:oulianov/Editor.git
cd Editor
git switch editor-ux
git submodule update --init
make
make run
```

liblcf, which reads and writes the games, is the submodule `lib/liblcf` (a
liblcf with the Maniac Patch support), built with the Editor: check it out
with `git submodule update --init` (or clone with `--recursive`), and again
after a pull that changes it. CMake checks it out when it is missing.

`make` detects your system and builds the Editor with everything it needs:

- **macOS**: needs Xcode's command line tools (`xcode-select --install`) and
  [Homebrew](https://brew.sh). The first `make` installs what is missing
  with Homebrew, then builds the Editor; later runs only rebuild what
  changed.
- **Linux**: needs [vcpkg](https://github.com/microsoft/vcpkg) (`VCPKG_ROOT`
  set), which builds the libraries (with Kirigami, Breeze and KDDockWidgets
  for the interface), ICU (`libicu-dev`) and Qt 6.10 or later (6.11.2 from
  qt.io or aqt: `make QT_DIR=~/Qt/6.11.2/gcc_64`).
- **Windows**: use the `windows-*` CMake presets.

`make test` builds and runs the tests, `make clean` removes the build. More
in [Building](docs/Building.md).


## Where it comes from

This is a fork of the [EasyRPG Editor](https://github.com/EasyRPG/Editor),
part of the [EasyRPG Project](https://easyrpg.org). Upstream is a Qt 6
rewrite of an editor for EasyRPG games that can import RPG Maker projects.
It reads and writes RPG Maker's own files with
[liblcf](https://github.com/EasyRPG/liblcf): games stay playable in RPG_RT,
DynRPG and EasyRPG Player, and stay editable in RPG Maker.


## Documentation

- [Building](docs/Building.md): what `make` does on each system, CMake
  options, tests.
- [Working with rpgsync](docs/rpgsync.md): the workflows with the RPG Maker
  editor and with this one.
- [Automation](docs/Automation.adoc): the script runner used by the tests.
- [UI state](docs/UiState.adoc): what the Editor remembers between sessions.
- [Updates](docs/Updates.adoc): how the Editor updates itself, for packagers.
- [Translation](docs/Translation.adoc): translating the Editor.

Upstream's wiki documents the original editor: https://wiki.easyrpg.org
