# EasyRPG Editor (oulianov fork)

A modern, multiplatform editor for RPG Maker 2000 and 2003 games.

## Install

```bash
git clone git@github.com:oulianov/Editor.git
cd Editor
make
make run
```

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


## What is this project?

An editor for RPG Maker 2000 and 2003 games, built on the
[EasyRPG Editor](https://github.com/EasyRPG/Editor) and aiming at what
RPG Maker 2003 itself does, plus a few things it never did: working side by
side with [rpgsync](https://github.com/oulianov/rpgsync), exporting games for
every platform and the web, and a modern, consistent interface.

It reads and writes RPG Maker's own files with
[liblcf](https://github.com/EasyRPG/liblcf): games stay playable in RPG_RT,
DynRPG and the EasyRPG Player, and stay editable in RPG Maker.


## Where it comes from

This is a fork of the [EasyRPG Editor](https://github.com/EasyRPG/Editor),
part of the [EasyRPG Project](https://easyrpg.org). Upstream is a Qt 6
rewrite of an editor for EasyRPG games that can import RPG Maker projects. 


## Using this editor with rpgsync

[rpgsync](https://github.com/oulianov/rpgsync) keeps a game and a folder of
Python scripts in sync: events become code you can edit, test and diff. With
this Editor, turn on the **rpgsync mode** in Settings and keep `rpgsync`
running: both sides can edit the game at the same time. Details in
[Working with rpgsync](docs/rpgsync.md).


## Documentation

- [Building](docs/Building.md): what `make` does on each system, CMake
  options, tests.
- [Working with rpgsync](docs/rpgsync.md): the workflows with the RPG Maker
  editor and with this one.
- [Automation](docs/Automation.adoc): the script runner used by the tests.
- [UI state](docs/UiState.adoc): what the Editor remembers between sessions.
- [Translation](docs/Translation.adoc): translating the Editor.

Upstream's wiki documents the original editor: https://wiki.easyrpg.org


## Bug reporting

For this fork: https://github.com/oulianov/Editor/issues

## License

EasyRPG Editor is free software available under the GPLv3 license. See the
file [COPYING](COPYING) for license conditions. For Author information see
the [AUTHORS document](docs/AUTHORS.adoc).

### 3rd party resources

The files in [resources/icons](resources/icons) are Breeze icons or derivates
licensed under LGPLv3. See the file
[COPYING-ICONS](docs/licenses/COPYING-ICONS) for license conditions.

The pixel fonts in [src/common/bitmapfont](src/common/bitmapfont) come from
EasyRPG Player, to preview text as the Player draws it:

* [Shinonome] font family (Japanese) - Copyright (c) 1999-2000 Yasuyuki
  Furukawa and contributors, provided under public domain, with the changes
  of EasyRPG Player (public domain)
* [ttyp0] font family - Copyright (c) 2012-2015 Uwe Waldmann, provided under
  ttyp0 license
* RMG2000-compatible font of EasyRPG Player

[Shinonome]: http://openlab.ring.gr.jp/efont/shinonome
[ttyp0]: https://people.mpi-inf.mpg.de/~uwe/misc/uw-ttyp0
