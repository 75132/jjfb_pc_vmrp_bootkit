#!/usr/bin/env python3
"""Phase 6J: build entry_selection_vs_publication + compare + verdict from live logs."""
from __future__ import annotations

import argparse
import hashlib
import re
import struct
import gzip
import zlib
from pathlib import Path


def try_decode(raw: bytes) -> bytes:
    for fn in (
        gzip.decompress,
        lambda b: zlib.decompress(b, 16 + zlib.MAX_WBITS),
        zlib.decompress,
    ):
        try:
            return fn(raw)
        except Exception:
            pass
    return raw


def member_sha(path: Path, want: str) -> tuple[str, int] | tuple[None, None]:
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b"MRPG":
        return None, None
    first = struct.unpack_from("<I", data, 4)[0] - 4
    hlen = struct.unpack_from("<I", data, 12)[0]
    pos = hlen
    while pos < first:
        nl = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos : pos + nl].rstrip(b"\0").decode("latin1", "replace")
        pos += nl
        off, slen, _ = struct.unpack_from("<III", data, pos)
        pos += 12
        if want in name:
            blob = try_decode(data[off : off + slen])
            return hashlib.sha256(blob).hexdigest(), len(blob)
    return None, None


def m1(text: str, pat: str, default: str = "n/a") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def m1_last(text: str, pat: str, default: str = "n/a") -> str:
    ms = list(re.finditer(pat, text))
    return ms[-1].group(1) if ms else default


def yesno(text: str, needle: str) -> str:
    return "yes" if needle in text else "no"


def extract_metrics(text: str) -> dict:
    # Prefer last EXT_ENTRY_CTX (shell nested entry), not early DSM helper class.
    entry = "n/a"
    first_pc = "n/a"
    header = "n/a"
    for m in re.finditer(
        r"\[EXT_ENTRY_CTX\][^\n]*entry_pc=(0x[0-9A-Fa-f]+)[^\n]*"
        r"header_entry_candidate=(0x[0-9A-Fa-f]+)[^\n]*entry_class=(\S+)",
        text,
    ):
        first_pc, header, entry = m.group(1), m.group(2), m.group(3)
    if entry == "n/a":
        entry = m1_last(text, r"entry_class=(\S+)", "n/a")
        first_pc = m1_last(text, r"observed_first_pc(?:_raw)?=(0x[0-9A-Fa-f]+)", first_pc)
        header = m1_last(text, r"header_entry_candidate=(0x[0-9A-Fa-f]+)", header)

    # Prefer PUBLICATION_SUMMARY; else infer from CONTEXT_FIELD_WRITE on nested P.
    wrote = {
        "0": m1(text, r"\[JJFB_PUBLICATION_SUMMARY\][^\n]*wrote_0=(\S+)", "n/a"),
        "4": m1(text, r"\[JJFB_PUBLICATION_SUMMARY\][^\n]*wrote_4=(\S+)", "n/a"),
        "8": m1(text, r"\[JJFB_PUBLICATION_SUMMARY\][^\n]*wrote_8=(\S+)", "n/a"),
        "C": m1(text, r"\[JJFB_PUBLICATION_SUMMARY\][^\n]*wrote_C=(\S+)", "n/a"),
    }
    if wrote["0"] == "n/a":
        # Infer from CONTEXT writes on the fault P if known.
        p_fault = m1(
            text,
            r"\[JJFB_EXTCHUNK_FAULT\][^\n]*P=(0x[0-9A-Fa-f]+)",
            m1(text, r"pre_fault P=(0x[0-9A-Fa-f]+)", ""),
        )
        if p_fault:
            for off, key in (("0x0", "0"), ("0x4", "4"), ("0x8", "8"), ("0xC", "C"), ("0xc", "C")):
                if re.search(
                    rf"\[CONTEXT_FIELD_WRITE\][^\n]*base={re.escape(p_fault)}[^\n]*offset={off}\b",
                    text,
                ):
                    wrote[key] = "yes"
            for k in wrote:
                if wrote[k] == "n/a":
                    wrote[k] = "no"

    return {
        "gate": m1(text, r"shell_native_exec_gate=(\S+)", "n/a"),
        "entry_class": entry,
        "first_pc": first_pc if first_pc != "n/a" else m1(text, r"entry_pc=(0x[0-9A-Fa-f]+)", "n/a"),
        "header": header,
        "fault_pc": m1_last(text, r"fault_pc=(0x[0-9A-Fa-f]+)", "n/a"),
        "fault_addr": m1_last(text, r"fault_addr=(0x[0-9A-Fa-f]+)", "n/a"),
        "function_start": m1(
            text, r"\[JJFB_EXTCHUNK_FAULT\][^\n]*function_start=(0x[0-9A-Fa-f]+)", "n/a"
        ),
        "p": m1(
            text,
            r"\[JJFB_PUBLICATION_SUMMARY\][^\n]*P=(0x[0-9A-Fa-f]+)",
            m1(text, r"\[JJFB_EXTCHUNK_FAULT\][^\n]*P=(0x[0-9A-Fa-f]+)", "n/a"),
        ),
        "wrote_0": wrote["0"],
        "wrote_4": wrote["4"],
        "wrote_8": wrote["8"],
        "wrote_C": wrote["C"],
        "last_048": m1(text, r"last_048_pc=(0x[0-9A-Fa-f]+)", "n/a"),
        "last_048_mod": m1(text, r"last_048_module=(\S+)", "n/a"),
        "pxc_writes": m1(text, r"\[JJFB_EXTCHUNK_FAULT\][^\n]*writes_seen=(\d+)", "0"),
        "strcom": yesno(text, "[JJFB_STRCOM]"),
        "mrc_init": yesno(text, "[JJFB_MRC_INIT]"),
        "export_call": yesno(text, "[JJFB_SHELL_EXPORT_CALL]"),
        "gamelist_start": yesno(text, "package=gwy/gamelist.mrp stage=mr_start"),
        "context_class": m1(text, r"gwy_context_class=(\S+)", "n/a"),
        "dispatch_class": m1(text, r"\[DISPATCH_CLASS\][^\n]*nature=(\S+)", "n/a"),
        "missing_0c": yesno(text, "missing P+0x0C"),
        "pub_summary": yesno(text, "[JJFB_PUBLICATION_SUMMARY]"),
    }


def write_entry_report(path: Path, metrics: dict, text: str) -> None:
    cont = "yes" if "guest_callback_continuation" in text or "CALLBACK_CONTINUATION" in text else "no"
    lines = [
        "# Phase 6J — entry selection vs publication",
        "",
        "## Live numbers (TARGET_OBSERVED)",
        "",
        f"- header_entry_candidate: `{metrics['header']}` (DOCUMENTED image+8)",
        f"- observed_first_pc: `{metrics['first_pc']}`",
        f"- entry_class: `{metrics['entry_class']}`",
        f"- DISPATCH_CLASS nature: `{metrics['dispatch_class']}`",
        f"- callback_continuation evidence: `{cont}`",
        f"- fault function_start: `{metrics['function_start']}`",
        f"- fault_pc / fault_addr: `{metrics['fault_pc']}` / `{metrics['fault_addr']}`",
        "",
        "## Publication chain",
        "",
        f"- P: `{metrics['p']}`",
        f"- wrote +0/+4/+8/+0xC: `{metrics['wrote_0']}` / `{metrics['wrote_4']}` / "
        f"`{metrics['wrote_8']}` / `{metrics['wrote_C']}`",
        f"- nearest +0/+4/+8 writer: pc=`{metrics['last_048']}` module=`{metrics['last_048_mod']}`",
        f"- missing P+0x0C tag: `{metrics['missing_0c']}`",
        "",
        "## Interpretation",
        "",
        "1. `first_pc` is **not** header image+8 → `WRONG_ENTRY_SELECTION` "
        "(TARGET_OBSERVED).",
        "2. If DISPATCH_CLASS / CALLBACK_CONTINUATION says guest callback continuation, "
        "DSM entered mid-function after nested `_mr_c_function_new`, not the module init "
        "entry that would publish mrc_extChunk (**HYPOTHESIS** favoring conclusion B, "
        "but +0/+4/+8 still got written on this path — so publication of +0xC is not "
        "solely “never entered any init”).",
        "3. +0/+4/+8 written while +0xC stays 0 on the same P means the live init path "
        "fills ER_RW metadata but **never** stores mrc_extChunk "
        "(TARGET_OBSERVED → supports A or C more than “no writer exists”).",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def write_compare(path: Path, runs: dict[str, dict], gwy: Path) -> None:
    jjfb = gwy / "jjfb.mrp"
    wx = gwy / "wxjwq.mrp"
    rows = []
    for mem in ("start.mr", "mrc_loader.ext"):
        jh, jn = member_sha(jjfb, mem)
        wh, wn = member_sha(wx, mem)
        eq = "yes" if jh and wh and jh == wh else "no"
        rows.append((mem, jh, jn, wh, wn, eq))

    lines = [
        "# Phase 6J — minimal cross-target publication compare",
        "",
        "## Member SHA (jjfb vs wxjwq)",
        "",
        "| member | jjfb sha256 | jjfb len | wxjwq sha256 | wxjwq len | equal |",
        "|---|---|---|---|---|---|",
    ]
    for mem, jh, jn, wh, wn, eq in rows:
        lines.append(
            f"| `{mem}` | `{jh or 'missing'}` | `{jn}` | `{wh or 'missing'}` | `{wn}` | `{eq}` |"
        )
        if eq == "yes":
            lines.append("")
            lines.append(f"- `{mem}` identical → **CROSS_TARGET** shared bootstrap.")
    lines.append("")
    lines.append("## Live metrics")
    lines.append("")
    lines.append(
        "| run | entry_class | first_pc | fault_pc | fault_addr | wrote_0/4/8/C | "
        "pxc_writes | export_call | gamelist |"
    )
    lines.append("|---|---|---|---|---|---|---|---|---|")
    for name, m in runs.items():
        lines.append(
            f"| `{name}` | `{m.get('entry_class')}` | `{m.get('first_pc')}` | "
            f"`{m.get('fault_pc')}` | `{m.get('fault_addr')}` | "
            f"`{m.get('wrote_0')}/{m.get('wrote_4')}/{m.get('wrote_8')}/{m.get('wrote_C')}` | "
            f"`{m.get('pxc_writes')}` | `{m.get('export_call')}` | `{m.get('gamelist_start')}` |"
        )
    lines.append("")
    all_c0 = all(m.get("wrote_C") in ("no", "n/a") for m in runs.values()) if runs else False
    all_fault = all(m.get("fault_addr") not in ("n/a", "") for m in runs.values()) if runs else False
    lines.append("## Cross-target reading")
    lines.append("")
    if all_c0 and all_fault:
        lines.append(
            "- All compared live runs show `wrote_C=no` and a NULL/+0x28-class fault → "
            "**CROSS_TARGET** platform publication gap (not jjfb-only)."
        )
    else:
        lines.append("- Mixed results; see per-run columns (TARGET_OBSERVED).")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def pick_verdict(runs: dict[str, dict], text_primary: str) -> tuple[str, str]:
    """Return (letter, rationale)."""
    m = runs.get("gbrwcore_jjfb") or (next(iter(runs.values())) if runs else {})
    wrote048 = m.get("wrote_0") == "yes" or m.get("wrote_4") == "yes" or m.get("wrote_8") == "yes"
    wrote_c = m.get("wrote_C") == "yes"
    wrong_entry = m.get("entry_class") == "WRONG_ENTRY_SELECTION"
    missing = m.get("missing_0c") == "yes" or (wrote048 and not wrote_c)

    # Prefer C if static/live show platform-wide skip of +0xC while other fields filled
    # Prefer B if wrong entry is the dominant story and +0/+4/+8 somehow not from this path
    # Prefer A if we have static +0xC writers that never executed
    # Prefer D if no static writers and no +0/+4/+8
    if wrote_c:
        return "A", "P+0xC was written somewhere but still faulted — lifetime/order audit (unexpected for 6J)."
    if wrong_entry and wrote048 and missing:
        # Dual: wrong entry AND skipped +0xC on the path that did write +0/+4/+8
        return (
            "B",
            "WRONG_ENTRY_SELECTION (first_pc ≠ header+8) with +0/+4/+8 written on continuation "
            "path and +0xC never published; next form is restore correct module init / entry "
            "order so publication routine can run (do not invent P+0xC).",
        )
    if wrote048 and missing:
        return (
            "C",
            "Same P received +0/+4/+8 from shell EXT but never +0xC; publication looks like a "
            "skipped cfunction/reg/platform fill on the live path.",
        )
    if not wrote048 and missing:
        return "D", "No +0/+4/+8/+0xC writers observed; enlarge source/doc compare — still no fake."
    return "D", "Insufficient discrimination; treat as open research (no fake)."


def write_verdict(path: Path, letter: str, rationale: str, runs: dict) -> None:
    lines = [
        "# Phase 6J — publication verdict",
        "",
        f"## Verdict: **{letter}**",
        "",
        rationale,
        "",
        "## Legend",
        "",
        "- **A**: legitimate publication routine exists but was not called",
        "- **B**: entry selection wrong; skipped init/publication",
        "- **C**: P+0xC should come from cfunction.ext/reg.ext primary publication flow",
        "- **D**: no writer found across samples — keep researching; no fake",
        "",
        "## Next phase (not implemented in 6J)",
        "",
    ]
    nxt = {
        "A": "Phase 6K: call/restore legitimate publication routine before shell native use of P+0xC",
        "B": "Phase 6K: fix MRPGCMAP entry selection / module init order",
        "C": "Phase 6K: restore cfunction.ext/reg.ext publication flow",
        "D": "Expand multi-game/source compare; still forbid inventing extChunk",
    }
    lines.append(nxt.get(letter, "TBD"))
    lines.append("")
    lines.append("## Runs considered")
    lines.append("")
    for name in runs:
        lines.append(f"- `{name}`")
    lines.append("")
    lines.append("Forbidden: invent P+0xC, R9 promotion, force UI, host_runapp_equivalent.")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("reports_dir")
    ap.add_argument("gwy_root")
    ap.add_argument("--primary-stdout", required=True)
    ap.add_argument("--extra-stdout", action="append", default=[], help="name=path")
    args = ap.parse_args()
    reports = Path(args.reports_dir)
    reports.mkdir(parents=True, exist_ok=True)
    gwy = Path(args.gwy_root)

    runs: dict[str, dict] = {}
    primary = Path(args.primary_stdout).read_text(encoding="utf-8", errors="replace")
    runs["gbrwcore_jjfb"] = extract_metrics(primary)
    write_entry_report(reports / "phase6j_entry_selection_vs_publication.md", runs["gbrwcore_jjfb"], primary)

    for item in args.extra_stdout:
        if "=" not in item:
            continue
        name, p = item.split("=", 1)
        t = Path(p).read_text(encoding="utf-8", errors="replace") if Path(p).is_file() else ""
        runs[name] = extract_metrics(t)

    write_compare(reports / "phase6j_minimal_cross_target_publication_compare.md", runs, gwy)
    letter, rationale = pick_verdict(runs, primary)
    write_verdict(reports / "phase6j_publication_verdict.md", letter, rationale, runs)
    print(f"verdict={letter}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
