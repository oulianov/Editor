# Translations from the RPG Maker 2003 editor

RPG Maker 2003 was translated by fans in many languages (RPG Advocate in
English, Rabbi-Bodom in French...). Their users know its terms. This folder
reuses those translations for the EasyRPG Editor.

- `mapping.json`: for each string of the Editor (Qt context and English source),
  the strings of RPG Maker 2003 with the same meaning, or why it has none.
- `rpgmaker_strings.py` and `dfm.py`: read the texts of an RPG Maker editor
  executable (its Delphi forms, string table and dialogs). The file is only
  read, never run.
- `make_ts.py`: fills a translation file of the Editor from a translated
  RPG Maker editor, using the mapping.
- `corrections_fr.json`: mistakes of the French translation, fixed when it is used
  (`keys`: texts to replace; `replace`: beginnings of words to replace
  everywhere, like capital letters without accents).

## How the strings are linked

Every text of RPG Maker has a key that does not depend on the language: the
form, the path of the component and the property (for example
`TFORMEDEVENT|FormEDEvent/.../CheckBox1|Caption`), or its id in the string
table. Translators only changed the texts, so the keys are the same in every
translated editor of the same version.

Each entry of `mapping.json` has:

- `context`, `source`: the string of the Editor, as in the `.ts` files;
- `rpgmaker`: the keys with the same meaning, the best first (the first one
  the translator translated is used);
- `rpgmaker_en`: the English text of the first key, in the reference editor
  (RPG Maker 2003 1.08, RPG Advocate), to read the mapping;
- `how`: how the link was made:
  - `command code` / `move code`: the button of the same command in RPG
    Maker's command list or move route window;
  - `same text, same window`: the same English text in the RPG Maker window
    that corresponds to the Editor's dialog;
  - `same text, French already chosen from it`, `French chosen from RPG Maker`:
    links recovered from the first French translation;
  - `reviewed`: linked by reading both editors, the wording differs;
- or `editor_only` with `why`: the Editor has no equivalent in RPG Maker
  (its own features, shortcuts, developer texts...). These are translated by hand.

`reference_texts` holds the English text of every key used, to tell which
strings a translator left in English.

## Adding a language

1. Create the translation file of the Editor (from the Editor folder):

       lupdate src -ts translations/editor_es.ts -target-language es -locations relative

2. Fill it from a translated RPG Maker 2003 editor:

       cd translations/rpgmaker
       uv run --with pefile python3 make_ts.py "/path/to/RPG2003.EXE" ../editor_es.ts

   Use `--colon ":"` for languages without a space before colons, and
   `--corrections corrections_es.json` to fix mistakes of that translation
   (`{"keys": {key: text}, "replace": {word: word}}`).

3. Translate the rest by hand (the strings marked `editor_only`, the ones the
   translator left in English) with Qt Linguist, then add the file to
   `TRANSLATION_SOURCES` in `CMakeLists.txt` and to `translations/editor.qrc`.

The French file can be made this way from the Rabbi-Bodom translation
(`RPG Maker 2003.exe`):

    uv run --with pefile python3 make_ts.py "RPG Maker 2003.exe" ../editor_fr.ts --corrections corrections_fr.json

## Keeping the mapping up to date

New strings of the Editor are not in the mapping: `make_ts.py` reports them as
"not in the mapping". Add an entry for each (link it to the keys of the RPG
Maker window it reproduces, or mark it `editor_only`). To find keys, extract
the reference editor and search its texts:

    uv run --with pefile python3 rpgmaker_strings.py RPG2003.EXE rm_en.json
