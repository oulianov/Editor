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


## The map tree

rpgsync writes the map tree (`RPG_RT.lmt`) as
`Scripts/database/map_tree.py`. It has one `Map(...)` or `Area(...)` entry per
map and area. The maps under a map go in the `with` block that follows its
entry, so the file has the shape of the tree. `start` is where a new game
begins.

```python
hertepay = Map(id=5, name="Hertepay")
with hertepay:
    relm = Map(id=1, name="Relm")
    with relm:
        cellar = Area(id=26, name="Cellar", area_rect=[2, 3, 9, 8])
```

What you change there reaches the game and, in rpgsync mode, the Editor's map
tree:

- **Rename** a map (`name=`): the tree and the tab of the open map show the
  new name, and the title of its script follows.
- **Add** an entry: rpgsync creates an empty 20x15 `MapNNNN.lmu` and its
  script, and the map appears in the tree.
- **Delete** an entry and its `with` block, which holds the maps under it:
  their `MapNNNN.lmu` files and scripts are deleted, and their tabs close. A
  copy stays in `Scripts/.rpgsync/history`.
  - A map with unsaved changes stays open, marked *(deleted)*. Saving it asks
    whether to put it back in the tree or to discard the changes.
  - The Editor never writes a map file that the tree doesn't list.
- **Move** a map (its entry and its block, elsewhere in the tree): the tree
  follows.
- Nothing goes under an area: rpgsync refuses it.

The other way, a map renamed in the Editor (Map Properties) has its new name
written to `RPG_RT.lmt` at once, without saving the map, so `map_tree.py`
follows. Its other changes, the start positions among them, wait for the map
to be saved. Area Properties are written at once.


## Open in Script Editor

In a list of event commands (an event page, a common event), **Open in Script
Editor** (right-click, or Ctrl+Shift+O) opens the script at the line of the
selected command; the event window has an **Open Script** button.

In the map tree (right-click), a map has **Open in Script Editor** (its
`MapNNNN.py`) and **Open Map Tree Entry in Script Editor** (its line in
`map_tree.py`), an area the second one, and the project **Open Map Tree in
Script Editor** (`map_tree.py` at its `start`).

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
