#!/usr/bin/env python3
"""E10A-3.1g: probe offset 0x349 for expected GPT tag across candidate buffers."""
from __future__ import annotations

from pathlib import Path

from phase6j_common import member_blob


def show(label: str, data: bytes | None, off: int = 0x349) -> None:
    if data is None:
        print(f"{label}: MISSING")
        return
    if off + 8 > len(data):
        print(f"{label}: len=0x{len(data):X} too small for off 0x{off:X}")
        return
    chunk = data[off : off + 16]
    asci = "".join(chr(c) if 32 <= c < 127 else "." for c in chunk)
    print(f"{label}: len=0x{len(data):X} @0x{off:X} hex={chunk[:8].hex()} ascii={asci!r}")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    off = 0x349
    mrp_gl = root / "game_files/mythroad/320x480/gwy/gamelist.mrp"
    mrp_jj = root / "game_files/mythroad/320x480/gwy/jjfb.mrp"
    show("gamelist.ext", member_blob(mrp_gl, "gamelist.ext"), off)
    show("gamelist.mrp raw", mrp_gl.read_bytes() if mrp_gl.exists() else None, off)
    show("jjfb.mrp raw", mrp_jj.read_bytes() if mrp_jj.exists() else None, off)
    for name in ("cfunction.ext", "start.mr", "logo.mrp"):
        pass
    # search GPT occurrences near start of gamelist.ext
    blob = member_blob(mrp_gl, "gamelist.ext")
    if blob:
        idx = 0
        hits = []
        while True:
            j = blob.find(b"GPT", idx)
            if j < 0:
                break
            hits.append(j)
            idx = j + 1
            if len(hits) >= 12:
                break
        print("GPT hits in gamelist.ext file_off:", [hex(h) for h in hits])
        # Also check if any buffer starts with GPT at 0x349-relative
        if blob[0x349 : 0x349 + 3] == b"GPT":
            print("gamelist.ext[0x349:0x34C] == GPT")
        else:
            print("gamelist.ext[0x349:0x34C] != GPT:", blob[0x349 : 0x349 + 3])

    # Decode 0x2E3180 range limit: 0x87<<5 = 0x10E0
    print("range_limit:", hex(0x87 << 5), "id+len:", hex(0x349 + 3))

    # Literal at 0x2E31B4 from earlier dump: 7811FFFF -> offset -0xEE88?
    lit = int.from_bytes(blob[0x2E31B4 - 0x2D4354 : 0x2E31B8 - 0x2D4354], "little", signed=True)
    # ADD r1,pc at 0x2E31A0: pc_read = 0x2E31A4; r1 = lit + 0x2E31A4
    # Actually LDR at 0x2E319E loads lit, then ADD r1,pc at 0x2E31A0 uses PC=0x2E31A4
    base = (0x2E31A0 + 4) & ~3  # unused
    pc_add = 0x2E31A0 + 4
    # Thumb ADD Rd,pc uses PC+4
    r1 = (lit + pc_add) & 0xFFFFFFFF
    print(f"0x2E3180 global_ptr materialize: lit={lit} (0x{lit & 0xFFFFFFFF:X}) r1=0x{r1:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
