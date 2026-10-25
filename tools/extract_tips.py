# Extract tutorial-tip source strings from the user's farm.wrp.exe into a data file
# (id <TAB> kind <TAB> text, with \n escaped). The port looks these up in sys/strings.xml.
import struct, sys
exe, out = sys.argv[1], sys.argv[2]
d = open(exe, "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]; ns = struct.unpack_from("<H", d, pe+6)[0]; osz = struct.unpack_from("<H", d, pe+20)[0]
secs = [struct.unpack_from("<8sIIII", d, pe+24+osz+i*40) for i in range(ns)]
def off(va):
    rva = va - 0x400000
    for n, vs, va_, rs, ra in secs:
        if va_ <= rva < va_ + max(vs, rs): return rva - va_ + ra
def wstr(va):
    o = off(va); e = o
    while d[e:e+2] != b"\0\0": e += 2
    return d[o:e].decode("utf-16-le")
# tip id, kind, address  (from the decompiled call sites)
SITES = [
    (0, "pet", 0x6250e0), (1, "pet", 0x624ee8), (2, "pet", 0x624a98), (3, "pet", 0x624c80), (4, "pet", 0x625518),
    (5, "bear", 0x624100), (0x1d, "bearfly", 0x624088), (6, "product", 0x61f888), (7, "cage", 0x621d60),
    (8, "depotfly", 0x621108), (9, "house", 0x622498), (10, "house", 0x6206b8), (0xb, "house", 0x622ba0),
    (0xc, "house", 0x620448), (0xd, "house", 0x620930), (0x10, "truck", 0x6228e8), (0x11, "wellempty", 0x61e088),
    (0x12, "wellauto", 0x623900), (0x13, "depotempty", 0x61f780), (0x16, "upgrade", 0x61f1a8),
    (0x19, "ui", 0x626ec8), (0x20, "uifly", 0x626f48), (0x22, "ui", 0x626e88), (0x1c, "pethungry", 0x61fec0),
]
with open(out, "w", encoding="utf-8") as f:
    for tid, kind, va in SITES:
        s = wstr(va).replace("\r", "").replace("\n", "\\n")
        f.write("%d\t%s\t%s\n" % (tid, kind, s))
