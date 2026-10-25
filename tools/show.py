# show.py Class [method ...] : print de-noised decompiled functions
import re, sys, os
# decompiled output of organize.py; override with $FF_DECOMP
d = os.environ.get("FF_DECOMP", os.path.join(os.path.dirname(__file__), "..", "decompiled", "game"))
cls = sys.argv[1]; methods = sys.argv[2:]
src = open(os.path.join(d, cls + ".c")).read() if os.path.exists(os.path.join(d, cls + ".c")) else open(os.path.join(d, "_unnamed.c")).read()
chunks = re.split(r"(?=^// ==== )", src, flags=re.M)
noise = re.compile(r"ExceptionList|puStack_c|^\s*undefined[0-9]? |local_18 = DAT|local_14 = |/\* WARNING|_free\(|= 0xf;$|& 0xffffff00\);|0xf < local|7 < local|local_8 = |local_8\._|^\s*void \*local_10;|^\s*uint local_10[0-9a-f]{2};|^\s*int local_8;|^\s*\}\s*$|^\s*$|^\s*if \(.*\) \{$\n\s*\}")
for c in chunks:
    m = re.match(r"// ==== (\S+)", c)
    if not m: continue
    name = m.group(1)
    if methods and not any(name == cls + "__" + x or name == x or name.startswith(cls + "__" + x + "_") for x in methods): continue
    lines = [l for l in c.split("\n") if not noise.search(l)]
    print("\n".join(lines))
