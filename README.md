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
| macOS (Apple silicon) | `EasyRPG-Editor-…-macOS-arm64.dmg` | Open it and drag **EasyRPG Editor** into Applications. |
| macOS (Intel) | `EasyRPG-Editor-…-macOS-x86_64.dmg` | Open it and drag **EasyRPG Editor** into Applications. |
| Windows | `EasyRPG-Editor-…-Windows.zip` | Unzip it anywhere, then open the Editor in the `bin` folder of **EasyRPG Editor**. |
| Linux | `EasyRPG_Editor-x86_64.AppImage` (or `aarch64`) | Make it executable (`chmod +x EasyRPG_Editor-*.AppImage`) and open it. |
| Linux (Flatpak) | `EasyRPG-Editor.flatpak` | `flatpak install EasyRPG-Editor.flatpak` |

The Editor updates itself: an **Update** button shows up when a new version
is out. To test your game, it uses [EasyRPG Player](https://easyrpg.org/player/),
which it finds on your computer or downloads for you.

Want to build it yourself? See [DEVELOPER.md](DEVELOPER.md).


## What it does better than RPG Maker 2003

- **Publish your game on the web.** Export your game for Windows, macOS,
  Linux and the web, with EasyRPG Player and the RTP files it uses. Each
  export is a .zip that's easy to upload to itch.io, so people can play your
  game in their browser, even on their phone.
- **Search everywhere.** Find everywhere a switch, a variable or a piece of
  text is used across the project. Every list is searchable too: type a few
  letters to find a switch, a variable, an item or an event by its number or
  its name.
- **Both layers side by side.** Widen the palette and the lower and upper
  tiles show next to each other. Picking a tile switches to its layer.
- **Messages without the 4-line limit.** Write as much text as you want: a
  long text is split into several message boxes for you. Every message code
  and special symbol is one click away.
- **Maniac Patch and EasyRPG Player custom commands.** RPG Maker has tons of
  mods and patches, like the
  [Maniac Patch](https://bingshan1024.github.io/steam2003_maniacs/), DynRPG
  or the EasyRPG Player commands. The Editor has advanced support for all
  these configurations and patches.
- **Raw editing and Python scripts.** Edit any command's raw values, or open
  an event as Python code with [rpgsync](https://github.com/oulianov/rpgsync).

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
