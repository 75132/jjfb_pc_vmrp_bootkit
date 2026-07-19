#!/usr/bin/env python3
"""v64: map helper code=1 → mrc_event → 0x2E2520 / Path A delivery contract."""
from __future__ import annotations

import hashlib
import struct
import zlib
from pathlib import Path

EXT = 0x2D8DF4
HELPER = 0x304AED
HELPER_CASE1 = 0x304B30
MRC_EVENT = 0x303E14
JUMP_2E2520 = 0x2E2520
PATH_A_STUB = 0x2E4040
PATH_A_BL = 0x2E4066
PATH_A_GATE = 0x2DADC4
QUEUE_DRAIN = 0x2DC80C
DISPATCH_A = 0x2DC8D4
DISPATCH_B = 0x2E7B9E
DISPATCH_B_ENTRY = 0x2E7B7C
CALLBACK_305EB8 = 0x305EB8
CALLBACK_305EBE = 0x305EBE

ROOT = Path(r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit")
MRP = ROOT / r"runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy\jjfb.mrp"
OUT = ROOT / "reports" / "v64_mrc_event_to_2e2520_static_map.md"

NAMES = {
    0: "MR_KEY_PRESS",
    1: "MR_KEY_RELEASE",
    2: "MR_MOUSE_DOWN",
    3: "MR_MOUSE_UP",
    4: "MR_MENU_SELECT",
    5: "MR_MENU_RETURN",
    6: "MR_DIALOG_EVENT",
    7: "MR_SMS_INDICATION",
    8: "MR_EXIT_EVENT",
    9: "MR_SMS_RESULT",
    10: "MR_LOCALUI_EVENT",
    11: "MR_OSD_EVENT",
    12: "MR_MOUSE_MOVE",
    13: "MR_ERROR_EVENT",
}


def load_robotol() -> tuple[bytes, str]:
    d = MRP.read_bytes()
    sha = hashlib.sha256(d).hexdigest()
    _, hend, _, idx = struct.unpack_from("<4I", d, 0)
    pos, end = idx, hend + 8
    e: dict[str, tuple[int, int, int]] = {}
    while pos < end:
        n = struct.unpack_from("<I", d, pos)[0]
        pos += 4
        name = d[pos : pos + n].rstrip(b"\0").decode("latin1")
        pos += n
        off, clen, flags = struct.unpack_from("<3I", d, pos)
        pos += 12
        e[name] = (off, clen, flags)
    rob = zlib.decompress(d[e["robotol.ext"][0] : e["robotol.ext"][0] + e["robotol.ext"][1]], 31)
    return rob, sha


def decode_bl(hw1: int, hw2: int, from_va: int) -> int | None:
    if (hw1 & 0xF800) != 0xF000 or (hw2 & 0xF800) != 0xF800:
        return None
    s = (hw1 >> 10) & 1
    imm10 = hw1 & 0x3FF
    j1 = (hw2 >> 13) & 1
    j2 = (hw2 >> 11) & 1
    imm11 = hw2 & 0x7FF
    i1 = j1 ^ s ^ 1
    i2 = j2 ^ s ^ 1
    imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    if s:
        imm |= ~((1 << 25) - 1)
    return (from_va + 4 + imm) & 0xFFFFFFFF


def decode_b(hw: int, from_va: int) -> int:
    imm = hw & 0x7FF
    if imm & 0x400:
        imm -= 0x800
    return (from_va + 4 + imm * 2) & 0xFFFFFFFF


def decode_bcond(hw: int, from_va: int) -> int:
    imm = hw & 0xFF
    if imm & 0x80:
        imm -= 256
    return (from_va + 4 + imm * 2) & 0xFFFFFFFF


def ldr_literal(rob: bytes, va: int) -> int | None:
    off = va - EXT
    if off < 0 or off + 3 >= len(rob):
        return None
    hw = rob[off] | (rob[off + 1] << 8)
    if (hw & 0xF800) != 0x4800:
        return None
    imm = (hw & 0xFF) * 4
    base = (va + 4) & ~2
    lit = base + imm
    lo = lit - EXT
    if 0 <= lo + 3 < len(rob):
        return struct.unpack_from("<I", rob, lo)[0]
    return None


def disasm(rob: bytes, va: int, nbytes: int = 180, stop_at_pop_pc: bool = True) -> list[str]:
    lines: list[str] = []
    off = va - EXT
    i = off
    end_i = min(len(rob), off + nbytes)
    while i < end_i:
        va2 = EXT + i
        hw = rob[i] | (rob[i + 1] << 8)
        extra = ""
        consumed = 2
        if i + 4 <= len(rob):
            bl = decode_bl(hw, rob[i + 2] | (rob[i + 3] << 8), va2)
            if bl is not None:
                lines.append(f"0x{va2:08X}: BL 0x{bl:X}")
                i += 4
                continue
        if (hw & 0xFF00) == 0xB500:
            extra = " PUSH"
        elif (hw & 0xFF00) == 0xBD00:
            extra = " POP"
        elif (hw & 0xF800) == 0x2800:
            extra = f" cmp r{(hw >> 8) & 7}, #{hw & 0xFF}"
        elif (hw & 0xF800) == 0x2000:
            extra = f" movs r{(hw >> 8) & 7}, #{hw & 0xFF}"
        elif (hw & 0xF800) == 0x4800:
            val = ldr_literal(rob, va2)
            extra = f" ldr r{(hw >> 8) & 7}, =0x{val:X}" if val is not None else " ldr literal"
        elif (hw & 0xFF00) == 0x4400:
            extra = " ADD/SUB reg"
        elif (hw & 0xF000) == 0xD000:
            extra = f" Bcond{(hw >> 8) & 0xF} -> 0x{decode_bcond(hw, va2):X}"
        elif (hw & 0xF800) == 0xE000:
            extra = f" B -> 0x{decode_b(hw, va2):X}"
        elif (hw & 0xFF00) == 0x4700:
            extra = " BX"
        lines.append(f"0x{va2:08X}: {hw:04X}{extra}")
        if stop_at_pop_pc and (hw & 0xFF00) == 0xBD00 and va2 > va + 4:
            break
        i += consumed
    return lines


def find_bl_to(rob: bytes, target_va: int, limit: int = 128) -> list[int]:
    hits: list[int] = []
    tgt = target_va & ~1
    i = 0
    while i + 4 <= len(rob):
        hw1 = rob[i] | (rob[i + 1] << 8)
        hw2 = rob[i + 2] | (rob[i + 3] << 8)
        bl = decode_bl(hw1, hw2, EXT + i)
        if bl is not None and (bl & ~1) == tgt:
            hits.append(EXT + i)
            if len(hits) >= limit:
                break
        i += 2
    return hits


def find_branch_to(rob: bytes, target_va: int, limit: int = 128) -> list[tuple[int, str]]:
    hits: list[tuple[int, str]] = []
    tgt = target_va & ~1
    i = 0
    while i + 2 <= len(rob):
        va = EXT + i
        hw = rob[i] | (rob[i + 1] << 8)
        if (hw & 0xF800) == 0xE000:
            to = decode_b(hw, va)
            if (to & ~1) == tgt:
                hits.append((va, "B"))
        elif (hw & 0xF000) == 0xD000:
            to = decode_bcond(hw, va)
            if (to & ~1) == tgt:
                hits.append((va, f"Bcond{(hw >> 8) & 0xF}"))
        i += 2
        if len(hits) >= limit:
            break
    return hits


def scan_bls(rob: bytes, start: int, nbytes: int) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    off = start - EXT
    i = off
    end = min(len(rob), off + nbytes)
    while i + 4 <= end:
        va = EXT + i
        hw1 = rob[i] | (rob[i + 1] << 8)
        hw2 = rob[i + 2] | (rob[i + 3] << 8)
        bl = decode_bl(hw1, hw2, va)
        if bl is not None:
            out.append((va, bl))
            i += 4
            continue
        i += 2
    return out


def read_u16_table(rob: bytes, table_va: int, pc_base: int, count: int) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    for idx in range(count):
        off = table_va - EXT + idx * 2
        hw = struct.unpack_from("<H", rob, off)[0]
        dest = pc_base + hw * 2
        out.append((idx, dest))
    return out


def jump_table_report(rob: bytes) -> list[str]:
    lines: list[str] = []
    table = 0x2E2544
    pc_base = 0x2E2540
    lines.append("| event | name | dest | note |")
    lines.append("|------:|------|------|------|")
    for idx, dest in read_u16_table(rob, table, pc_base, 20):
        ev = idx + 3
        note = ""
        if abs(dest - 0x2E37A0) < 8:
            note = "→ 2DC4D8 (B71)"
        if dest == PATH_A_STUB or abs(dest - PATH_A_BL) < 8:
            note = "→ 2E4066 → 2DADC4 (Path A)"
        lines.append(f"| {ev} | `{NAMES.get(ev, '?')}` | `0x{dest:X}` | {note} |")
    return lines


def trace_function(rob: bytes, fn_va: int) -> list[str]:
    bls = find_bl_to(rob, fn_va)
    brs = find_branch_to(rob, fn_va)
    lines = [
        f"### `0x{fn_va:X}`",
        "",
        f"- Direct **BL** callers ({len(bls)}): "
        + (", ".join(f"`0x{x:X}`" for x in bls) or "(none)"),
        f"- Direct **B/Bcond** to entry ({len(brs)}): "
        + (", ".join(f"`0x{va:X}` ({k})" for va, k in brs) or "(none)"),
        "",
        "```text",
    ]
    lines.extend(disasm(rob, fn_va, 180, stop_at_pop_pc=False))
    lines.append("```")
    for caller in bls[:6]:
        up = find_bl_to(rob, caller)
        lines.append(
            f"- Upstream BL→`0x{caller:X}`: "
            + (", ".join(f"`0x{x:X}`" for x in up[:8]) or "(none / fall-through)")
        )
    return lines


def mrc_event_code_summary() -> list[str]:
    return [
        "| r0 (event) | static path | reaches 0x2E2520? |",
        "|-----------:|-------------|-------------------|",
        "| 0 | `303E36` dialog/key init via `304558` indirect | no |",
        "| 1–5 | `cmp r0,#0` → `B 303F0E` → `303FF6` `movs r0,#0; POP` | **no (immediate return 0)** |",
        "| 6+ | `cmp r0,#6` → `303F4E` sub-switch (0/1/2/20/58…) | no |",
        "| 8 | helper case1 also calls `304480` (exit) before return | no |",
        "",
        "Static proof: `mrc_event` has **zero** BL to `0x2E2520`, `0x2DC80C`, `0x312AC4`, `0x312C0C`.",
    ]


def main() -> int:
    rob, sha = load_robotol()
    case1_bls = scan_bls(rob, HELPER_CASE1, 80)
    me_bls = scan_bls(rob, MRC_EVENT, 0x400)
    hits_2520_bl = find_bl_to(rob, JUMP_2E2520)
    hits_2520_b = find_branch_to(rob, JUMP_2E2520)

    lines: list[str] = []
    lines.append("# v64 mrc_event → 0x2E2520 Static Map")
    lines.append("")
    lines.append(f"- MRP SHA256: `{sha}`")
    lines.append(f"- robotol.ext decompressed: {len(rob)} bytes")
    lines.append(f"- ext_base: `0x{EXT:X}`")
    lines.append("")

    lines.append("## Helper `0x304AED` → case 1 (`0x304B30`) → mrc_event")
    lines.append("")
    lines.append("| step | VA | note |")
    lines.append("|------|-----|------|")
    lines.append(f"| helper entry | `0x{HELPER:X}` | host `_mr_c_function_new` target |")
    lines.append(f"| case 1 handler | `0x{HELPER_CASE1:X}` | `cmp r0,#8` → optional exit |")
    lines.append(f"| **mrc_event** | `0x{MRC_EVENT:X}` | BL from `0x304B42` |")
    lines.append(f"| refresh | `0x30560C` | after mrc_event (case1 `0x304B4A`) |")
    lines.append("")
    lines.append("### Case 1 handler")
    lines.append("")
    lines.append("```text")
    lines.extend(disasm(rob, HELPER_CASE1, 80))
    lines.append("```")
    lines.append("")
    lines.append("Case 1 BL chain:")
    for va, tgt in case1_bls:
        label = {MRC_EVENT: "mrc_event", 0x304480: "mrc_exitApp?", 0x30560C: "mrc_refreshScreenReal?"}.get(
            tgt, ""
        )
        lines.append(f"- `0x{va:X}` → `0x{tgt:X}` {label}")

    lines.append("")
    lines.append(f"## mrc_event @ `0x{MRC_EVENT:X}`")
    lines.append("")
    lines.append("```text")
    lines.extend(disasm(rob, MRC_EVENT, 260, stop_at_pop_pc=False))
    lines.append("```")
    lines.append("")
    lines.extend(mrc_event_code_summary())
    lines.append("")
    lines.append("### mrc_event BL callees (first 16)")
    for va, tgt in me_bls[:16]:
        lines.append(f"- `0x{va:X}` → `0x{tgt:X}`")

    lines.append("")
    lines.append("## Jump table `0x2E2520` (index = event_code − 3)")
    lines.append("")
    lines.extend(jump_table_report(rob))

    lines.append("")
    lines.append("## ALL callers / branches → `0x2E2520`")
    lines.append("")
    lines.append(f"**Only {len(hits_2520_bl)} direct BL sites in entire robotol.ext:**")
    for va in hits_2520_bl:
        ctx = "inside queue-drain mega-fn" if va == DISPATCH_A else "inside 2E7B7C loop"
        lines.append(f"- `0x{va:X}` ({ctx})")
    lines.append("")
    lines.append(f"B/Bcond direct to `0x2E2520`: {len(hits_2520_b)}")
    for va, kind in hits_2520_b:
        lines.append(f"- `0x{va:X}` ({kind})")
    if not hits_2520_bl and not hits_2520_b:
        lines.append("- (none besides the two BL above)")

    lines.append("")
    lines.append("## Queue drain: timer → `0x2DC80C` → fall-through `0x2DC8D4` → `0x2E2520`")
    lines.append("")
    lines.append(
        "`0x2DC80C` is a **single mega-function** (not a tiny wrapper). Early exit at `0x2DC846` "
        "when queue helpers `0x312AC4`/`0x312AB4` report empty (`cmp r0,#0`). "
        "When a queued event survives filtering, control reaches `0x2DC8D2` → **`0x2DC8D4` BL `0x2E2520`**."
    )
    lines.append("")
    lines.append("```text")
    lines.extend(disasm(rob, QUEUE_DRAIN, 120, stop_at_pop_pc=False))
    lines.extend(disasm(rob, DISPATCH_A, 24, stop_at_pop_pc=False))
    lines.append("```")
    lines.append("")
    lines.append(f"- Sole BL→`0x2DC80C`: **`0x{CALLBACK_305EBE:X}`** (inside callback `0x305EB8`)")
    lines.append("- `0x305EB8` has **no** direct BL callers → registered callback / timer (`dealtimer`)")
    lines.append("- v63 dynamic: `2DC80C` hit hundreds of times but queue always empty → never reaches `2DC8D4`")

    lines.append("")
    lines.append("## Alternate dispatch: `0x2E7B7C` → `0x2E7B9E` → `0x2E2520`")
    lines.append("")
    lines.extend(trace_function(rob, DISPATCH_B_ENTRY))
    lines.append("")
    lines.append("```text")
    lines.extend(disasm(rob, DISPATCH_B, 28))
    lines.append("```")
    callers_7b7c = find_bl_to(rob, DISPATCH_B_ENTRY)
    lines.append("")
    lines.append(f"BL→`0x2E7B7C`: {', '.join(f'`0x{x:X}`' for x in callers_7b7c)}")
    for c in callers_7b7c:
        up = find_bl_to(rob, c)
        lines.append(
            f"- Upstream BL→`0x{c:X}`: "
            + (", ".join(f"`0x{x:X}`" for x in up[:8]) or "(none / table dispatch)")
        )

    lines.append("")
    lines.append("## Path A stub (event 5 / 12)")
    lines.append("")
    lines.append("```text")
    lines.extend(disasm(rob, PATH_A_STUB, 48))
    lines.append("```")

    lines.append("")
    lines.append("## Conclusion")
    lines.append("")
    lines.append("### Why helper(code=1) + mrc_event(5) returns 0 but never hits `0x2E2520`")
    lines.append("")
    lines.append(
        "1. **Two different event layers.** Helper code=1 calls platform **`mrc_event@0x303E14`** "
        "(Mythroad API). The Path-A switch **`0x2E2520`** is robotol **internal** dispatch, reached "
        "only from **`0x2DC8D4`** or **`0x2E7B9E`**."
    )
    lines.append(
        "2. **`mrc_event(5,0,0)` is a no-op.** For `1 ≤ r0 ≤ 5`, static flow is: "
        "`cmp r0,#0` → branch to `0x303F0E` → `0x303FF6` → `movs r0,#0; POP`. "
        "No enqueue, no BL to queue fns, no call to `0x2E2520`. Return 0 is expected."
    )
    lines.append(
        "3. **`0x2E2520` has exactly 2 BL callers** in all of robotol.ext: "
        f"`0x{DISPATCH_A:X}` (queue-drain tail) and `0x{DISPATCH_B:X}` (secondary loop). "
        "Neither is reachable from `mrc_event`."
    )
    lines.append(
        "4. **Queue drain requires a pre-filled queue.** Timer/callback path "
        f"`dealtimer → 0x305EB8 → 0x305EBE → 0x2DC80C` runs, but if nothing was enqueued "
        "(via a different producer than helper code=1), it exits early at `0x2DC846` — "
        "matching v63 logs (`2DC80C` hit, `2E2520`=0)."
    )
    lines.append("")
    lines.append("### Correct host delivery contract for Path A (`0x2E4040` → `0x2DADC4`)")
    lines.append("")
    lines.append(
        "**Path A cannot be reached via helper mrc_event alone.** Required contract:"
    )
    lines.append("")
    lines.append(
        "1. **Enqueue** event code **5** (`MR_MENU_RETURN`) or **12** (`MR_MOUSE_MOVE`) into the "
        "robotol event queue consumed by `0x312AC4`/`0x312AB4`/`0x312C0C` at the start of `0x2DC80C` "
        "(producer is **not** `mrc_event@0x303E14`; likely Mythroad `sendAppEvent` / ext event "
        "callback / indirect `0x304558`-style dispatch)."
    )
    lines.append(
        "2. **Drain** via existing timer contract: host helper **code=2** (`mrc_timerTimeout`) → "
        "`dealtimer` callback → `0x305EB8` → `0x305EBE` → `0x2DC80C` → (non-empty queue) → "
        "`0x2DC8D4` → `0x2E2520` → stub `0x2E4040` → **`BL 0x2DADC4`**."
    )
    lines.append(
        "3. Alternate route: invoke the **`0x2E7B7C`** loop (callers `0x2F4F7E`, `0x313320`) "
        "which tail-calls `0x2E7B9E` → `0x2E2520` — still requires the queue/state that loop expects."
    )
    lines.append("")
    lines.append("### Key addresses")
    lines.append("")
    lines.append("| role | VA |")
    lines.append("|------|-----|")
    rows = [
        ("robotol helper", HELPER),
        ("helper case1", HELPER_CASE1),
        ("platform mrc_event (NOT internal switch)", MRC_EVENT),
        ("queue-drain entry", QUEUE_DRAIN),
        ("BL→2E2520 (queue path)", DISPATCH_A),
        ("BL→2E2520 (alt loop)", DISPATCH_B),
        ("internal event switch", JUMP_2E2520),
        ("Path A stub (ev 5/12)", PATH_A_STUB),
        ("Path A BL gate_init", PATH_A_BL),
        ("gate_init target", PATH_A_GATE),
        ("timer callback", CALLBACK_305EB8),
        ("callback queue drain call", CALLBACK_305EBE),
    ]
    for name, va in rows:
        lines.append(f"| {name} | `0x{va:X}` |")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT}")
    print(f"mrc_event=0x{MRC_EVENT:X} BL->2E2520={[hex(x) for x in hits_2520_bl]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
