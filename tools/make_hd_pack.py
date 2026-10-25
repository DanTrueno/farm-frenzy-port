# Build hd.pack from AI-upscaled textures.
#   make_hd_pack.py <original_png_dir> <upscaled_flat_dir> <out.pack> [scale]
# original_png_dir: textures decoded to PNG, mirroring the pack paths ("pets/cow/eat.dds.png")
# upscaled_flat_dir: model output with '/' replaced by '@@' in file names (4x)
# Each texture is resized to exactly scale x the original size (Lanczos) so sprite-sheet
# frame grids stay aligned, then stored as PNG under its original pack path.
import io, os, re, sys, zipfile
import numpy as np
from PIL import Image, ImageFilter

# Backgrounds: the model invents texture there (cracked earth on the field, a dark seam on the
# menu rainbow). For these the base comes from the original (Lanczos) and only 35% of the
# model's fine detail is added on top.
SOFT = re.compile(r"^(back\.dds|ui/bakgr000\.dds|heaven\d+\.dds|ui/map/grass\d+\.dds)\.png$")


def soften(orig, up, size):
    base = np.asarray(orig.convert("RGBA").resize(size, Image.LANCZOS)).astype(np.float32)
    blur = np.asarray(up.filter(ImageFilter.GaussianBlur(3))).astype(np.float32)
    det = np.asarray(up).astype(np.float32) - blur
    return Image.fromarray(np.clip(base + 0.35 * det, 0, 255).astype(np.uint8))

orig_dir, up_dir, out, scale = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 3
n = skipped = soft = 0
with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as z:
    for root, _, files in os.walk(orig_dir):
        for f in sorted(files):
            if not f.endswith(".png"):
                continue
            rel = os.path.relpath(os.path.join(root, f), orig_dir)
            src = os.path.join(up_dir, rel.replace(os.sep, "@@"))
            orig = Image.open(os.path.join(root, f))
            w, h = orig.size
            if min(w, h) < 8 or not os.path.exists(src):  # tiny UI bits: keep the original
                skipped += 1
                continue
            size = (w * scale, h * scale)
            up = Image.open(src).convert("RGBA").resize(size, Image.LANCZOS)
            if SOFT.match(rel.replace(os.sep, "/")):
                up = soften(orig, up, size)
                soft += 1
            buf = io.BytesIO()
            up.save(buf, "PNG", optimize=False, compress_level=6)
            z.writestr(rel[:-4].replace(os.sep, "/"), buf.getvalue())  # drop the ".png" suffix
            n += 1
print("packed", n, "soft", soft, "skipped", skipped, "->", out, "%.0f MB" % (os.path.getsize(out) / 1e6))
