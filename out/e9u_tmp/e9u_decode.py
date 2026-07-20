#!/usr/bin/env python3
"""E9U: decode 2FC03C object (R9+0x11EC+0x24) and BA0+0x2C writers."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from e8w_f6c_object_xref import CODE_BASE, decode, find_fn_start, find_bl_callers

blob = (ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext").read_bytes()
OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)


def dis(start: int, end: int, label: str) -> list[str]:
    lines = [f"===== {label} 0x{start:X}..0x{end:X} ====="]
    pc = start
    while pc < end:
        sz, text, meta = decode(blob, pc)
        m = ""
        bl = meta.get("bl")
        if bl is not None:
            m += f"  ; BL->0x{bl:X}"
        if "STR" in text:
            m += "  ; STORE"
        lit = meta.get("lit")
        if lit in (0x11EC, 0xBA0, 0xBCC) or "#0x2C" in text:
            m += "  ; INTEREST"
        lines.append(f"0x{pc:08X}: {text}{m}")
        pc += sz
    return lines


def step_back(pc: int):
    for d in (2, 4):
        if pc - d < CODE_BASE:
            continue
        sz, text, meta = decode(blob, pc - d)
        if sz == d:
            return pc - d, text, meta
    return None


lines: list[str] = []

callers = find_bl_callers(blob, 0x2FC03C)
lines.append(f"callers_2FC03C={[hex(c) for c in callers]}")
for c in callers:
    s = find_fn_start(blob, c)
    lines += dis(s, max(s + 0xA0, c + 0x20), f"caller_{c:X}_fn")
    lines += dis(max(CODE_BASE, c - 0x20), c + 0x20, f"near_BL_{c:X}")

s = find_fn_start(blob, 0x2FED14)
lines += dis(s, s + 0x140, "2FED14_fullish")
lines += dis(0x2FC03C, 0x2FC070, "2FC03C")
lines += dis(0x2EFA90, 0x2EFB20, "splash_progress_loop")

hits_11ec = []
hits_bcc = []
hits_str2c = []
ba0_sites = []
pc = CODE_BASE
end = CODE_BASE + len(blob)
while pc + 1 < end:
    sz, text, meta = decode(blob, pc)
    lit = meta.get("lit")
    if lit == 0x11EC or "0x11EC" in text:
        hits_11ec.append((pc, text))
    if lit == 0xBCC or "0xBCC" in text:
        hits_bcc.append((pc, text))
    if lit == 0xBA0 or "0xBA0" in text:
        ba0_sites.append(pc)
    if text.startswith("STR") and "#0x2C" in text and "[sp" not in text:
        hits_str2c.append((pc, text))
    pc += sz

lines.append("===== lit/use 0x11EC =====")
for p, t in hits_11ec:
    lines.append(f"0x{p:X}: {t}")
lines.append(f"count_11ec={len(hits_11ec)}")

lines.append("===== lit 0xBCC =====")
for p, t in hits_bcc:
    lines.append(f"0x{p:X}: {t}")
lines.append(f"count_bcc={len(hits_bcc)}")

# STR #0x2C preceded by BA0 base within 16 insn
lines.append("===== STR #0x2C with nearby BA0/BCC =====")
ba0_writers = []
for p, t in hits_str2c:
    back = []
    q = p
    hit = False
    for _ in range(20):
        prev = step_back(q)
        if not prev:
            break
        q, text, meta = prev
        back.append((q, text, meta))
        lit = meta.get("lit")
        if lit in (0xBA0, 0xBCC) or "0xBA0" in text or "0xBCC" in text:
            hit = True
            break
        # also: ADD rn, #0xBA0 pattern already covered by lit
    if hit:
        ba0_writers.append((p, t, list(reversed(back))))

for p, t, back in ba0_writers:
    lines.append(f"--- writer @0x{p:X} {t} ---")
    for q, text, meta in back:
        lines.append(f"  0x{q:X}: {text}")

# BA0-using functions that also STR #0x2C
lines.append("===== BA0 fns that STR #0x2C =====")
seen_fn = set()
for site in ba0_sites:
    s = find_fn_start(blob, site)
    if s in seen_fn:
        continue
    seen_fn.add(s)
    has2c = False
    window = []
    q = s
    limit = s + 0x120
    while q < limit:
        sz, text, meta = decode(blob, q)
        window.append(f"0x{q:X}: {text}")
        if "STR" in text and "#0x2C" in text and "[sp" not in text:
            has2c = True
        q += sz
        if q > s + 0x200:
            break
    if has2c:
        lines.append(f"--- fn_start=0x{s:X} ba0_site=0x{site:X} ---")
        lines.extend(window[:60])

# Who stores INTO 0x11EC+0x24 region? Look for STR with base built from 11EC
lines.append("===== near stores after 11EC load =====")
for p, t in hits_11ec:
    # dump 0x40 after
    lines += dis(p, p + 0x50, f"after_11EC_{p:X}")

out = OUT / "e9u_decode.txt"
out.write_text("\n".join(lines), encoding="utf-8")
print("wrote", out)
print("callers_2FC03C", [hex(c) for c in callers])
print("11ec", len(hits_11ec), "bcc", len(hits_bcc), "ba0_str2c", len(ba0_writers))
for p, t, _ in ba0_writers[:15]:
    print(f"  BA0+2C writer 0x{p:X}: {t}")
