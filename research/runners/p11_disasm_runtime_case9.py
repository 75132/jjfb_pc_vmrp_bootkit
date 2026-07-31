#!/usr/bin/env python3
"""P11: Thumb disasm of runtime Guest dumps for Case-9 / late fault."""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORTS = ROOT / "reports"

CASE9_BIN = REPORTS / "p11_runtime_case9_30d280_30d480.bin"
FAULT_BIN = REPORTS / "p11_runtime_fault_2d95c0_2d9660.bin"
CASE9_BASE = 0x30D280
FAULT_BASE = 0x2D95C0


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    h.update(p.read_bytes())
    return h.hexdigest()


def thumb_decode(pc: int, data: bytes, off: int) -> tuple[int, str]:
    """Minimal Thumb decoder for P11 reports. Returns (size, text)."""
    if off + 2 > len(data):
        return 0, "???"
    h0 = data[off] | (data[off + 1] << 8)
    # 32-bit Thumb
    if (h0 & 0xF800) in (0xE800, 0xF000, 0xF800) and off + 4 <= len(data):
        h1 = data[off + 2] | (data[off + 3] << 8)
        raw = f"{h0:04X} {h1:04X}"
        if (h0 & 0xF800) == 0xF000 and (h1 & 0xC000) == 0xC000:
            s = (h0 >> 10) & 1
            imm10 = h0 & 0x3FF
            j1 = (h1 >> 13) & 1
            j2 = (h1 >> 11) & 1
            imm11 = h1 & 0x7FF
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 |= 0xFE000000
                # sign extend
                imm32 = imm32 - (1 << 32) if imm32 & (1 << 31) else imm32
            tgt = (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)
            op = "BL" if (h1 & 0x1000) else "BLX"
            return 4, f"{raw}  {op} 0x{tgt:X}"
        return 4, f"{raw}  .thumb32"
    raw = f"{h0:04X}"
    # PUSH/POP
    if (h0 & 0xFE00) == 0xB400:
        return 2, f"{raw}  PUSH {{{h0 & 0x1FF:X}}}"
    if (h0 & 0xFE00) == 0xBC00:
        return 2, f"{raw}  POP {{{h0 & 0x1FF:X}}}"
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"{raw}  SUB SP, #0x{(h0 & 0x7F) << 2:X}"
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"{raw}  ADD SP, #0x{(h0 & 0x7F) << 2:X}"
    if (h0 & 0xF800) == 0x9800:
        rd = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        return 2, f"{raw}  LDR R{rd}, [SP, #0x{imm:X}]"
    if (h0 & 0xF800) == 0x9000:
        rd = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        return 2, f"{raw}  STR R{rd}, [SP, #0x{imm:X}]"
    if (h0 & 0xFF87) == 0x4700:
        return 2, f"{raw}  BX R{(h0 >> 3) & 7}"
    if (h0 & 0xFF87) == 0x4780:
        return 2, f"{raw}  BLX R{(h0 >> 3) & 7}"
    if (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
        imm = struct.unpack("b", bytes([h0 & 0xFF]))[0]
        tgt = pc + 4 + (imm << 1)
        return 2, f"{raw}  Bcond -> 0x{tgt:X}"
    if (h0 & 0xF800) == 0xE000:
        imm = h0 & 0x7FF
        if imm & 0x400:
            imm |= ~0x7FF
        tgt = pc + 4 + (imm << 1)
        return 2, f"{raw}  B -> 0x{tgt:X}"
    if (h0 & 0xF800) == 0x2000:
        return 2, f"{raw}  MOVS R{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"
    if (h0 & 0xF800) == 0x2800:
        return 2, f"{raw}  CMP R{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"
    if (h0 & 0xFFC0) == 0x4280:
        return 2, f"{raw}  CMP R{h0 & 7}, R{(h0 >> 3) & 7}"
    if (h0 & 0xF800) == 0x6800:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"{raw}  LDR R{rt}, [R{rn}, #0x{imm:X}]"
    if (h0 & 0xF800) == 0x6000:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"{raw}  STR R{rt}, [R{rn}, #0x{imm:X}]"
    # ADD Rd, Rm (high)
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"{raw}  ADD R{rd}, R{rm}"
    if (h0 & 0xFF00) == 0x4600:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"{raw}  MOV R{rd}, R{rm}"
    return 2, f"{raw}  .thumb"


def disasm_range(path: Path, base: int, out_path: Path, highlight: set[int] | None = None):
    data = path.read_bytes()
    lines = []
    lines.append(f"# Runtime Thumb disasm base=0x{base:X} len={len(data)} file={path.name}")
    lines.append("")
    off = 0
    blocks = []
    app9_entry = None
    while off + 2 <= len(data):
        pc = base + off
        size, text = thumb_decode(pc, data, off)
        if size == 0:
            break
        raw = data[off : off + size]
        raw_hex = " ".join(f"{b:02X}" for b in raw)
        mark = " <<<" if highlight and pc in highlight else ""
        lines.append(f"0x{pc:08X}: {raw_hex:<11}  {text}{mark}")
        if "CMP" in text and "#0x9" in text:
            app9_entry = pc
            lines.append(f"  ; possible app=9 compare at 0x{pc:X}")
        if text.startswith(tuple("B")) or "BL" in text or "BX" in text or "BLX" in text:
            blocks.append(pc)
        off += size
    if app9_entry is not None:
        lines.append("")
        lines.append(f"# app=9 compare candidate: 0x{app9_entry:X}")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out_path} ({len(lines)} lines)")


def main() -> int:
    missing = [p for p in (CASE9_BIN, FAULT_BIN) if not p.exists()]
    if missing:
        print("MISSING dumps:", ", ".join(str(p) for p in missing), file=sys.stderr)
        print("Run p11_run_natural_case9.ps1 first.", file=sys.stderr)
        return 1
    sha_path = REPORTS / "p11_runtime_dump_sha256.txt"
    sha_lines = [
        f"{CASE9_BIN.name} sha256={sha256_file(CASE9_BIN)}",
        f"{FAULT_BIN.name} sha256={sha256_file(FAULT_BIN)}",
    ]
    sha_path.write_text("\n".join(sha_lines) + "\n", encoding="utf-8")
    print("\n".join(sha_lines))
    disasm_range(CASE9_BIN, CASE9_BASE, REPORTS / "p11_runtime_case9_disasm.txt", {0x30D301})
    disasm_range(FAULT_BIN, FAULT_BASE, REPORTS / "p11_runtime_fault_disasm.txt", {0x2D960E})
    # Annotate 0x2D960E specifically
    fault = FAULT_BIN.read_bytes()
    off = 0x2D960E - FAULT_BASE
    if 0 <= off < len(fault) - 1:
        size, text = thumb_decode(0x2D960E, fault, off)
        raw = fault[off : off + size]
        note = REPORTS / "p11_runtime_fault_disasm.txt"
        with note.open("a", encoding="utf-8") as f:
            f.write("\n# Focus 0x2D960E\n")
            f.write(f"0x2D960E: {' '.join(f'{b:02X}' for b in raw)}  {text}\n")
            f.write("# Role of nearby 0x1E205 must be confirmed by dynamic slice / fault ring.\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
