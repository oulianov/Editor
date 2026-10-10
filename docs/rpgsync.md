# Working with rpgsync

[rpgsync](https://github.com/oulianov/rpgsync) keeps an RPG Maker 2000/2003
game and a folder of Python scripts (`Scripts/` in the game folder) in sync:
the events of each map and the common events become code you can edit,
search, test and keep in git, and saving a script writes it back into the
game.


## With the RPG Maker 2000/2003 editor

RPG Maker keeps its own copy of the game in memory and ignores the files that
change on disk; its next save writes that copy back. Work on one side at a
time:

```bash
rpgsync pull     # game -> scripts
# edit the Python files, RPG Maker closed
rpgsync push     # scripts -> game
```

then open RPG Maker and keep editing there.


## With this Editor

Turn on **Settings > rpgsync mode** and keep `rpgsync` running in the game
folder. Both can then edit the game at the same time:

- **What rpgsync writes appears in the Editor**: the database, the map tree
  and the open maps are read again when their files change. Windows editing
  that data (the database...) take the changes when they close.
- **Nothing is half-written**: the Editor writes each file next to it, then
  renames it over the old one, so rpgsync never reads half a file (and the
  Editor retries a file it catches while rpgsync writes it).
- **Nothing is silently overwritten**: when the Editor saves data whose file
  changed since it read it, it merges both sides, entry by entry:
  - the database: each entry of each list (heroes, common events...), the
    vocabulary, the system and the battle commands;
  - a map: each event, each tile, its other properties;
  - the map tree: each map, the order of the maps, the start positions.

  Each entry takes the side that changed it. Only an entry both sides changed
  differently is a conflict: the Editor then names it and asks whether to
  save its version, reload the file (dropping its changes) or cancel.


## Open in Script Editor

In a list of event commands (an event page, a common event), **Open in Script
Editor** (right-click, or Ctrl+Shift+O) opens the script at the line of the
selected command; the event window has an **Open Script** button.

The script editor is set in **Settings > Script editor**: a URL or a command,
with `{file}` and `{line}`:

| Editor | Setting |
|---|---|
| Visual Studio Code (default) | `vscode://file/{file}:{line}` |
| Cursor | `cursor://file/{file}:{line}` |
| A command | `code -g {file}:{line}`, `subl {file}:{line}`... |
| (empty) | the program of the system, at the start of the file |

The Editor asks rpgsync where the line is (`rpgsync locate`, from the script
as it is now), with the rpgsync of the game's `Scripts/.venv`.
