# Audit original RGB565 splash resources: raw vs 0xF81F keyed previews
import struct
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    import subprocess, sys
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow", "-q"])
    from PIL import Image

ROOT = Path(r"c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit")
CACHE = ROOT / "runtime" / "vmrp_win32" / "vmrp_win32_20220102" / "jjfb_bmp_cache"
OUT = ROOT / "reports"
OUT.mkdir(exist_ok=True)

ASSETS = [
    ("slogo!157!58.bmp", 157, 58),
    ("loadingbar!201!29.bmp", 201, 29),
    ("bar!16!18.bmp", 16, 18),
    ("textbar!120!30.bmp", 120, 30),
]

KEY = 0xF81F


def rgb565_to_rgb(p):
    r = ((p >> 11) & 0x1F) * 255 // 31
    g = ((p >> 5) & 0x3F) * 255 // 63
    b = (p & 0x1F) * 255 // 31
    return (r, g, b)


def load_rgb565(path, w, h):
    data = path.read_bytes()
    need = w * h * 2
    if len(data) < need:
        raise SystemExit(f"{path}: size {len(data)} < {need}")
    pix = list(struct.unpack("<" + "H" * (w * h), data[:need]))
    return pix


def to_img(pix, w, h, keyed=False):
    im = Image.new("RGBA", (w, h))
    px = im.load()
    skipped = 0
    for y in range(h):
        for x in range(w):
            p = pix[y * w + x]
            if keyed and p == KEY:
                px[x, y] = (0, 0, 0, 0)
                skipped += 1
            else:
                r, g, b = rgb565_to_rgb(p)
                px[x, y] = (r, g, b, 255)
    return im, skipped


def corners(pix, w, h):
    return [pix[0], pix[w - 1], pix[(h - 1) * w], pix[(h - 1) * w + w - 1]]


rows = []
raw_tiles = []
key_tiles = []
max_h = 0
for name, w, h in ASSETS:
    path = CACHE / (name + ".rgb565")
    if not path.exists():
        rows.append(f"| {name} | MISSING | | | | | |")
        continue
    pix = load_rgb565(path, w, h)
    c = corners(pix, w, h)
    nkey = sum(1 for p in pix if p == KEY)
    im_raw, _ = to_img(pix, w, h, False)
    im_key, skipped = to_img(pix, w, h, True)
    im_raw.save(OUT / f"v43_{name.replace('!', '_')}_raw.png")
    im_key.save(OUT / f"v43_{name.replace('!', '_')}_keyed.png")
    raw_tiles.append(im_raw)
    key_tiles.append(im_key)
    max_h = max(max_h, h)
    rows.append(
        f"| {name} | {w}x{h} | {[hex(x) for x in c]} | {nkey}/{w*h} ({100*nkey/(w*h):.1f}%) | "
        f"{'YES' if nkey else 'no'} | obj+1C/corners |"
    )
    print(name, "key_pixels", nkey, "corners", [hex(x) for x in c])

# contact sheets
pad = 8


def sheet(tiles, path):
    if not tiles:
        return
    tw = sum(t.size[0] for t in tiles) + pad * (len(tiles) + 1)
    th = max(t.size[1] for t in tiles) + pad * 2
    canvas = Image.new("RGBA", (tw, th), (40, 40, 40, 255))
    x = pad
    for t in tiles:
        canvas.paste(t, (x, pad), t if t.mode == "RGBA" else None)
        x += t.size[0] + pad
    canvas.save(path)


sheet(raw_tiles, OUT / "resource_contactsheet_raw.png")
sheet(key_tiles, OUT / "resource_contactsheet_keyed.png")

md = OUT / "resource_audit_v43.md"
md.write_text(
    "# Resource audit v43\n\n"
    "RGB565 cache under `jjfb_bmp_cache/`. Key = `0xF81F` (magenta).\n\n"
    "| name | size | corners | key pixels | has 0xF81F | note |\n"
    "|------|------|---------|------------|------------|------|\n"
    + "\n".join(rows)
    + "\n\nSee `resource_contactsheet_raw.png` / `resource_contactsheet_keyed.png`.\n",
    encoding="utf-8",
)
print("wrote", md)
