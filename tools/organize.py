# Split Ghidra output into functions, name them from embedded __FUNCSIG__ strings,
# rewrite FUN_xxx references and write per-class files.
import re, sys, os, collections
src = open(sys.argv[1], encoding="utf-8", errors="replace").read()
outdir = sys.argv[2]
parts = re.split(r"^// ==== (\S+) @ (\S+)\n", src, flags=re.M)
funcs = []  # (name, addr, body)
for i in range(1, len(parts), 3):
    funcs.append([parts[i], parts[i+1], parts[i+2]])
sig_re = re.compile(r'"((?:[\w:<>\*&, ]+ )?(c\w+|i\w+)::(~?\w+)\s*\([^"]*\))"')
names = {}
cls_of = {}
funcs.sort(key=lambda f: f[1])
# collect signatures from catch funclets into the preceding real function
owner = None
sigmap = collections.defaultdict(collections.Counter)
for f in funcs:
    if f[0].startswith(("Catch", "Unwind")):
        if owner is not None:
            for m in sig_re.finditer(f[2]): sigmap[owner][m.group(1)] += 1
    else:
        owner = f[0]
        for m in sig_re.finditer(f[2]): sigmap[owner][m.group(1)] += 1
for f in funcs:
    name, addr, body = f
    if not name.startswith("FUN_"): continue
    sigs = sigmap.get(name)
    if not sigs: continue
    sig = sigs.most_common(1)[0][0]
    m = sig_re.search('"%s"' % sig)
    cls, meth = m.group(2), m.group(3)
    new = "%s__%s" % (cls, meth.replace("~", "dtor_"))
    if new in names.values(): new += "_" + addr[-4:]
    names[name] = new
    cls_of[name] = cls
    f.append(sig)
print("named", len(names), "of", sum(1 for f in funcs if f[0].startswith("FUN_")))
rx = re.compile(r"\b(FUN_[0-9a-f]{8})\b")
by_class = collections.defaultdict(list)
for f in funcs:
    name, addr, body = f[0], f[1], f[2]
    body = rx.sub(lambda m: names.get(m.group(1), m.group(1)), body)
    nn = names.get(name, name)
    header = "// ==== %s @ %s%s\n" % (nn, addr, ("   // " + f[3]) if len(f) > 3 else "")
    by_class[cls_of.get(name, "_unnamed")].append(header + body)
for cls, items in by_class.items():
    open(os.path.join(outdir, cls + ".c"), "w").write("".join(items))
print(len(by_class), "files")
open(os.path.join(outdir, "_names.txt"), "w").write("\n".join("%s %s" % kv for kv in sorted(names.items())))
