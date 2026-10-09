"""Minimal binary DFM (TPF0) parser + PE resource reader. Pure data parsing."""
import struct, pefile

VA = dict(Null=0, List=1, Int8=2, Int16=3, Int32=4, Extended=5, String=6, Ident=7,
          False_=8, True_=9, Binary=10, Set=11, LString=12, Nil=13, Collection=14,
          Single=15, Currency=16, Date=17, WString=18, Int64=19, UTF8String=20, Double=21)

class Str:
    """Raw string value: keeps bytes + kind so the caller chooses the codec."""
    __slots__ = ("raw", "kind")
    def __init__(self, raw, kind): self.raw, self.kind = raw, kind
    def __repr__(self): return f"Str({self.kind},{self.raw!r})"

class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0
    def byte(self):
        b = self.d[self.p]; self.p += 1; return b
    def take(self, n):
        if self.p + n > len(self.d): raise ValueError(f"overrun at {self.p}+{n}")
        r = self.d[self.p:self.p+n]; self.p += n; return r
    def u32(self): return struct.unpack("<I", self.take(4))[0]
    def sstr(self):
        n = self.byte(); return self.take(n)
    def value(self):
        t = self.byte()
        if t == 0: return ("null", None)
        if t == 1:
            items = []
            while self.d[self.p] != 0: items.append(self.value())
            self.p += 1
            return ("list", items)
        if t == 2: return ("int", struct.unpack("<b", self.take(1))[0])
        if t == 3: return ("int", struct.unpack("<h", self.take(2))[0])
        if t == 4: return ("int", struct.unpack("<i", self.take(4))[0])
        if t == 5: return ("ext", self.take(10))
        if t == 6: return ("str", Str(self.sstr(), "ansi"))
        if t == 7: return ("ident", self.sstr().decode("latin-1"))
        if t == 8: return ("bool", False)
        if t == 9: return ("bool", True)
        if t == 10: return ("binary", self.take(self.u32()))
        if t == 11:
            s = []
            while True:
                x = self.sstr()
                if not x: break
                s.append(x.decode("latin-1"))
            return ("set", s)
        if t == 12: return ("str", Str(self.take(self.u32()), "ansi"))
        if t == 13: return ("nil", None)
        if t == 14:
            items = []
            while self.d[self.p] != 0:
                order = None
                if self.d[self.p] in (2, 3, 4): order = self.value()[1]
                if self.byte() != 1: raise ValueError(f"collection item without vaList at {self.p-1}")
                props = []
                while self.d[self.p] != 0:
                    name = self.sstr().decode("latin-1"); props.append((name, self.value()))
                self.p += 1
                items.append((order, props))
            self.p += 1
            return ("collection", items)
        if t == 15: return ("single", struct.unpack("<f", self.take(4))[0])
        if t == 16: return ("currency", self.take(8))
        if t == 17: return ("date", struct.unpack("<d", self.take(8))[0])
        if t == 18: return ("str", Str(self.take(self.u32()*2), "utf16"))
        if t == 19: return ("int", struct.unpack("<q", self.take(8))[0])
        if t == 20: return ("str", Str(self.take(self.u32()), "utf8"))
        if t == 21: return ("double", struct.unpack("<d", self.take(8))[0])
        raise ValueError(f"unknown value type {t} at {self.p-1}")
    def obj(self):
        flags, pos = 0, None
        if self.d[self.p] & 0xF0 == 0xF0:
            flags = self.byte() & 0x0F
            if flags & 2: pos = self.value()[1]
        cls = self.sstr().decode("latin-1"); name = self.sstr().decode("latin-1")
        props = []
        while self.d[self.p] != 0:
            pn = self.sstr().decode("latin-1"); props.append((pn, self.value()))
        self.p += 1
        children = []
        while self.d[self.p] != 0: children.append(self.obj())
        self.p += 1
        return dict(cls=cls, name=name, flags=flags, pos=pos, props=props, children=children)

def parse_dfm(data):
    if data[:4] != b"TPF0": raise ValueError("not TPF0")
    r = Reader(data); r.p = 4
    o = r.obj()
    return o, r.p

def load_resources(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
    out = {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tid = str(t.name) if t.name else t.id
        for e in t.directory.entries:
            n = str(e.name) if e.name else e.id
            for l in e.directory.entries:
                out.setdefault(tid, []).append((n, l.id, pe.get_data(l.data.struct.OffsetToData, l.data.struct.Size)))
    return out
