"""Extract the user interface texts of an RPG Maker 2000/2003 editor executable.

    uv run --with pefile python3 rpgmaker_strings.py RPG2003.EXE out.json

The executable is only read, never run. Texts come from its Delphi forms
(key: FORM|Component/Path|Property), its string table (STRINGTABLE|id) and its
dialog resources. Translated editors (RPG Advocate, Rabbi-Bodom...) keep the
same forms and components, so the keys are the same in every language.
"""
import os, sys, json, re, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dfm import *

TEXT_PROPS = {"Caption", "Hint", "Text", "Items.Strings", "Lines.Strings", "Tabs.Strings",
              "Filter", "Title", "DefaultExt", "Strings", "DisplayLabel", "EditLabel.Caption",
              "Panels", "Columns", "Sections", "HelpKeyword", "TextHint"}
SKIP_PROPS = {"Font.Name", "KeyName", "TitleFont.Name"}
HW_KATA = re.compile(r"[｡-ﾟ]")
CJK = re.compile(r"[぀-ヿ一-鿿]")

def fix_hw(s):
    # cp1252 high byte that was once read as Shift-JIS half-width katakana and saved as UTF-8 (e.g. 'ｰ' for '°')
    if HW_KATA.search(s) and not CJK.search(s):
        return HW_KATA.sub(lambda m: m.group(0).encode("shift_jis").decode("cp1252"), s)
    return s

def decode(v, issues, key):
    if v.kind == "utf16": s = v.raw.decode("utf-16-le")
    elif v.kind == "utf8": s = v.raw.decode("utf-8")
    else:
        try: s = v.raw.decode("cp1252")
        except UnicodeDecodeError:
            s = v.raw.decode("cp1252", errors="replace"); issues.append(("cp1252-undecodable", key, repr(v.raw)))
    s2 = fix_hw(s)
    if s2 != s: issues.append(("halfwidth-katakana-fixed", key, s + " -> " + s2)); s = s2
    if CJK.search(s): issues.append(("japanese-remnant", key, s))
    return s

def is_text_prop(p):
    last = p.split("].")[-1]
    return p not in SKIP_PROPS and (p in TEXT_PROPS or last in TEXT_PROPS or p.split("[")[0] in TEXT_PROPS)

def walk_form(form, o, path, out, other, issues):
    def rec(pn, k, v):
        key = f"{form}|{path}|{pn}"
        if k == "str":
            s = decode(v, issues, key)
            (out if is_text_prop(pn) else other)[key] = {"text": s, "src": "dfm", "cls": o["cls"]}
        elif k == "list":
            for i, (kk, vv) in enumerate(v): rec(f"{pn}[{i}]", kk, vv)
        elif k == "collection":
            for i, (order, ps) in enumerate(v):
                for p2, (k2, v2) in ps: rec(f"{pn}[{i}].{p2}", k2, v2)
    for pn, (k, v) in o["props"]: rec(pn, k, v)
    seen = {}
    for c in o["children"]:
        nm = c["name"] or f"<{c['cls']}>"
        seen[nm] = seen.get(nm, 0) + 1
        if seen[nm] > 1: nm += f"#{seen[nm]}"
        walk_form(form, c, path + "/" + nm, out, other, issues)

def string_tables(res, out):
    for name, lang, data in res.get(6, []):
        p = 0
        for i in range(16):
            n = struct.unpack_from("<H", data, p)[0]; p += 2
            s = data[p:p+2*n].decode("utf-16-le"); p += 2*n
            if n: out[f"STRINGTABLE|{(name-1)*16+i}"] = {"text": s, "src": "stringtable"}

def dialogs(res, out):
    # RT_DIALOG: only extract UTF-16 strings for title/controls (DLGTEMPLATE, non-EX)
    for name, lang, data in res.get(5, []):
        style, exstyle, cdit, x, y, cx, cy = struct.unpack_from("<IIHhhhh", data, 0)
        p = 18
        def sz_or_ord(p):
            w = struct.unpack_from("<H", data, p)[0]
            if w == 0xFFFF: return f"#{struct.unpack_from('<H', data, p+2)[0]}", p+4
            e = p
            while struct.unpack_from("<H", data, e)[0]: e += 2
            return data[p:e].decode("utf-16-le"), e+2
        menu, p = sz_or_ord(p); klass, p = sz_or_ord(p); title, p = sz_or_ord(p)
        out[f"DIALOG|{name}|title"] = {"text": title, "src": "dialog"}
        if style & 0x40:  # DS_SETFONT
            p += 2; font, p = sz_or_ord(p)
        for i in range(cdit):
            p = (p + 3) & ~3
            p += 18
            cls, p = sz_or_ord(p); txt, p = sz_or_ord(p)
            extra = struct.unpack_from("<H", data, p)[0]; p += 2 + extra
            if txt and not txt.startswith("#"): out[f"DIALOG|{name}|ctl{i}"] = {"text": txt, "src": "dialog"}

def menus(res, out):
    for name, lang, data in res.get(4, []):
        out[f"MENU|{name}|raw"] = {"text": data.hex(), "src": "menu-unparsed"}

def extract(exe):
    """The texts of exe: {key: {"text", "src", ...}}, plus the form names and decoding issues."""
    res = load_resources(exe)
    out, other, issues, forms = {}, {}, [], []
    for name, lang, data in res[10]:
        if data[:4] != b"TPF0": continue
        o, end = parse_dfm(data)
        assert end == len(data), name
        forms.append(name)
        walk_form(name, o, o["name"], out, other, issues)
    string_tables(res, out); dialogs(res, out); menus(res, out)
    return {"exe": exe, "forms": forms, "texts": out, "nontext_strings": other, "issues": issues}

if __name__ == "__main__":
    exe, dest = sys.argv[1], sys.argv[2]
    data = extract(exe)
    json.dump(data, open(dest, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(dest, "forms", len(data["forms"]), "texts", len(data["texts"]), "issues", len(data["issues"]))
