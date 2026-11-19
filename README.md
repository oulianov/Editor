# EasyRPG Editor (oulianov fork)

A modern editor for RPG Maker 2000 and 2003 games. It runs on macOS, Windows
and Linux, and it's open source. Huge shoutout to the
[EasyRPG community](https://easyrpg.org) and
[Ghabry](https://github.com/EasyRPG/Editor).

It reads and writes the same game files as RPG Maker 2000 and 2003, and it
keeps RPG Maker's habits, like the double right-click to switch layers.

![The EasyRPG Editor on macOS, editing a map](https://oulianov.github.io/blog/assets/easyrpg-editor/main-window.png)


## Download

Get the latest version from the
[releases page](https://github.com/oulianov/Editor/releases/latest):

| System | File | How to install |
|---|---|---|
| Linux | `EasyRPG_Editor-x86_64.AppImage` (or `aarch64`) | Make it executable (`chmod +x EasyRPG_Editor-*.AppImage`) and open it. |
| Linux (Flatpak) | `EasyRPG-Editor.flatpak` | `flatpak install EasyRPG-Editor.flatpak` |
| macOS | `EasyRPG-Editor-…-macOS-arm64.dmg` (Apple silicon) or `…-macOS-x86_64.dmg` (Intel) | Open it and drag **EasyRPG Editor** into Applications. |
| Windows | `EasyRPG-Editor-…-Windows.zip` | Unzip it anywhere, then open the Editor in the `bin` folder of **EasyRPG Editor**. |

The Editor updates itself: an **Update** button shows up when a new version
is out. To test your game, it uses [EasyRPG Player](https://easyrpg.org/player/),
which it finds on your computer or downloads for you.

Want to build it yourself? See [DEVELOPER.md](DEVELOPER.md).


## What it does better than RPG Maker 2003

### Publish your game on the web

Export for Windows, macOS, Linux and the web: a .zip ready for itch.io, playable in a browser, even on a phone.

![The Export Game window](https://oulianov.github.io/blog/assets/easyrpg-editor/export-game.png)

### Search everywhere

Find where a switch, a variable or a piece of text is used, and search every list by number or name.

![Searching the whole project](https://oulianov.github.io/blog/assets/easyrpg-editor/project-search.png)

### Both layers side by side

Widen the palette and the lower and upper tiles show next to each other.

![The palette with the lower and the upper layer side by side](https://oulianov.github.io/blog/assets/easyrpg-editor/palette-both-layers.png)

### Messages without the 4-line limit

Long texts are split into message boxes for you, and every message code is one click away.

![The Show Message window and its codes](https://oulianov.github.io/blog/assets/easyrpg-editor/message-codes.png)

### Maniac Patch and EasyRPG Player custom commands

Advanced support for the [Maniac Patch](https://bingshan1024.github.io/steam2003_maniacs/), DynRPG and the EasyRPG Player commands and options.

![Database, System page, with the EasyRPG Player options](https://oulianov.github.io/blog/assets/easyrpg-editor/database-system.png)

### Raw editing and Python scripts

Edit any command's raw values, or open an event as Python code with [rpgsync](https://github.com/oulianov/rpgsync).

![Right-click menu of the event command list](https://oulianov.github.io/blog/assets/easyrpg-editor/command-menu.png)

And all the other perks:

- **Test play** with EasyRPG Player, from the start or from any spot of the
  map.
- **Animated tiles and panoramas** in the map, as in the game.
- **More zoom levels**, and Ctrl + mouse wheel to zoom.
- **Panels you can move anywhere**, and a map tree wide enough for full map
  names, with a search field.
- **Map tabs** to keep several maps open.
- **Event commands colored by type**, so long events are easier to read.
- **Go to a map** by its ID, and **Ctrl+F** opens the search right away.
- **Jukebox and Resource Manager** to listen to the game's music and sounds.
- **MIDI music as players will hear it**, with the game's soundfont.
- **Light and dark themes**, in English or French.
- **Several RTP folders**, one for RPG Maker 2000 games and one for RPG
  Maker 2003 games.
- **Two editors side by side**: open two games at once.

More screenshots: [oulianov.github.io/blog/docs/easyrpg_editor](https://oulianov.github.io/blog/docs/easyrpg_editor/)


## Works with rpgsync

[rpgsync](https://github.com/oulianov/rpgsync) turns your game's events into
Python scripts and keeps them in sync with the game. Edit the Python code, and
the changes show up live in the Editor, which stays open and merges them with
yours. Details in [Working with rpgsync](docs/rpgsync.md).


## Thank you

Huge thank you to the
[Oniromancie](https://www.rpg-maker.fr/index.php?page=forum&id=29729) and
[E-magination](https://e-magination.jeun.fr/t5043-ressusciter-un-projet-rpg-maker-2003-en-2026)
community that helped me develop these tools: Medoc-, PM, Mimigris, Mack,
jetrotal, FwT, trotter, AlexRE, Relm...

It all relies on [liblcf](https://github.com/EasyRPG/liblcf), the amazing open
source library by the EasyRPG team that reads and writes RPG Maker game files,
and on [EasyRPG Player](https://easyrpg.org/player/) to play the games.


## Bug reports and ideas

Tell me if it works for you and how to improve it on
[GitHub](https://github.com/oulianov/Editor/issues)!


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
