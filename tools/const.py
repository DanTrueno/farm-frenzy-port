# const.py ADDR [ADDR...] : print float/double/int at virtual address in farm.wrp.exe
# the executable path comes from $FF_EXE (default: ./farm.wrp.exe)
import struct, sys, os
d = open(os.environ.get("FF_EXE", "farm.wrp.exe"), "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]; ns = struct.unpack_from("<H", d, pe+6)[0]; osz = struct.unpack_from("<H", d, pe+20)[0]
secs = [struct.unpack_from("<8sIIII", d, pe+24+osz+i*40) for i in range(ns)]
def off(va):
    rva = va - 0x400000
    for n, vs, vaa, rs, ra in secs:
        if vaa <= rva < vaa + max(vs, rs): return rva - vaa + ra
for a in sys.argv[1:]:
    va = int(a, 16); o = off(va)
    f = struct.unpack_from("<f", d, o)[0]; db = struct.unpack_from("<d", d, o)[0]; i = struct.unpack_from("<i", d, o)[0]
    print("%s float=%g double=%g int=%d" % (a, f, db, i))
