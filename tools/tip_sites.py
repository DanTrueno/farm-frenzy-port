# List tip call sites: (function, kind, tip id, string address)
import re, glob
for f in sorted(glob.glob("decompiled/game/*.c")):
    src = open(f).read()
    for ch in re.split(r"(?=^// ==== )", src, flags=re.M):
        m = re.match(r"// ==== (\S+)", ch)
        if not m: continue
        lines = ch.split("\n")
        for i, l in enumerate(lines):
            kind = "HELP" if "cHelpTips__cHelpTips(" in l else "FLY" if "cFlyingTips__cFlyingTips(" in l else None
            if not kind or l.lstrip().startswith(("cHelpTips__cHelpTips(undefined", "cFlyingTips__cFlyingTips(undefined")): continue
            ctx = "\n".join(lines[max(0, i-45):i+1])
            dats = re.findall(r"(?:FUN_00408a40|FUN_00406950|FUN_005a845d)\(&DAT_([0-9a-f]+)", ctx)
            ids = re.findall(r"FUN_004d0c30\([^,]+,(0x[0-9a-f]+|\d+)\)", ctx)
            print(m.group(1), kind, ids[-1] if ids else "-", dats[-1] if dats else "-")
