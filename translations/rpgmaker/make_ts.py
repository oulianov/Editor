"""Fill a translation of the Editor from a translated RPG Maker 2003 editor.

    uv run --with pefile python3 make_ts.py "RPG Maker 2003.exe" ../editor_fr.ts \
        [--corrections corrections_fr.json] [--overwrite]

mapping.json links each string of the Editor (context + English source) to the
strings of RPG Maker 2003 with the same meaning (keys of its forms and string
table, the same in every translated RPG Maker editor). For each linked string
of the .ts file, the text of the first linked key that the translator of that
editor translated is used, with the punctuation of the Editor's string.

The .ts file must exist: create or update it first with
    lupdate ../../src -ts ../editor_xx.ts -target-language xx -locations relative
Strings already translated are kept unless --overwrite is given.
"""
import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpgmaker_strings import extract  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TAIL = re.compile(r"(\s*:|\s*\.\.\.|…)\s*$")


def clean(text):
    """Without accelerators, surrounding spaces and the trailing colon or ellipsis."""
    text = text.replace("&&", "\0").replace("&", "").replace("\0", "&")
    return TAIL.sub("", re.sub(r"\s+", " ", text).strip()).strip()


def adapt(source, translated, colon=" :"):
    """The translation with the trailing colon / ellipsis and spaces of the source."""
    text = clean(translated)
    stripped = source.rstrip()
    if stripped.endswith("...") or stripped.endswith("…"):
        text += "..."
    elif stripped.endswith(":"):
        text += colon
    lead = source[:len(source) - len(source.lstrip())]
    trail = source[len(source.rstrip()):]
    return lead + text + trail


PRINTF = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?[sdiuxXc]")
QT = re.compile(r"%(\d+)")


def placeholders(source, text):
    """RPG Maker's %d / %s become the Editor's %1, %2... in order; None when they do not fit."""
    wanted = sorted({int(n) for n in QT.findall(source)})
    found = PRINTF.findall(text)
    if not wanted:
        return text if not found else None
    if len(found) != len(wanted) or wanted != list(range(1, len(wanted) + 1)):
        return None
    numbers = iter(wanted)
    return PRINTF.sub(lambda m: f"%{next(numbers)}", text)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("exe", help="a translated RPG Maker 2003 editor (read only, never run)")
    parser.add_argument("ts", help="the .ts file of the Editor to fill")
    parser.add_argument("--corrections", help='JSON {"keys": {key: text}, "replace": {word: word}} fixing that translation')
    parser.add_argument("--overwrite", action="store_true", help="replace the translations already in the file")
    parser.add_argument("--colon", default=" :", help='how the language writes a trailing colon (French: " :")')
    args = parser.parse_args()

    mapping = json.load(open(os.path.join(HERE, "mapping.json"), encoding="utf-8"))
    links = {(e["context"], e["source"]): e for e in mapping["strings"]}
    translated = {k: v["text"] for k, v in extract(args.exe)["texts"].items()}
    reference = mapping["reference_texts"]
    if args.corrections:
        # {"keys": {key: text}, "replace": {word: word}}: texts replaced, then
        # words replaced everywhere (e.g. missing accents)
        corrections = json.load(open(args.corrections, encoding="utf-8"))
        translated.update(corrections.get("keys", {}))
        for word, fixed in corrections.get("replace", {}).items():
            # at the start of words, so plurals and derived words are fixed too
            pattern = re.compile(r"\b" + re.escape(word))
            translated = {k: pattern.sub(fixed, v) for k, v in translated.items()}

    tree = ET.parse(args.ts)
    filled = kept = untranslated = editor_only = unmapped = 0
    for context in tree.getroot().iter("context"):
        name = context.findtext("name")
        for message in context.iter("message"):
            source = message.findtext("source") or ""
            translation = message.find("translation")
            if translation.get("type") in ("vanished", "obsolete"):
                continue
            entry = links.get((name, source))
            if entry is None:
                unmapped += 1
                continue
            if entry.get("editor_only"):
                editor_only += 1
                continue
            done = translation.get("type") not in ("unfinished", "vanished", "obsolete") and (translation.text or "").strip()
            if done and not args.overwrite:
                kept += 1
                continue
            # the first linked string that this translator translated
            text = None
            for key in entry["rpgmaker"]:
                value = translated.get(key, "")
                if clean(value) and clean(value) != clean(reference.get(key, "")):
                    text = value
                    break
            if text is not None:
                text = placeholders(source, text)
            if text is None:
                untranslated += 1
                continue
            translation.text = adapt(source, text, args.colon)
            translation.attrib.pop("type", None)
            filled += 1

    tree.write(args.ts, encoding="utf-8", xml_declaration=True)
    # the header lupdate writes
    data = open(args.ts, encoding="utf-8").read()
    data = data.replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>', 1)
    open(args.ts, "w", encoding="utf-8").write(data)
    print(f"filled {filled}, kept {kept}, not translated in that editor {untranslated}, "
          f"editor only {editor_only}, not in the mapping {unmapped}")


if __name__ == "__main__":
    main()
