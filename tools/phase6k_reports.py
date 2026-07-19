#!/usr/bin/env python3
"""Phase 6K: build reports from live stdout."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def m1(text: str, pat: str, default: str = "n/a") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def yesno(text: str, needle: str) -> str:
    return "yes" if needle in text else "no"


def classify_fault(text: str) -> str:
    entry_hit = "[JJFB_MRPGCMAP_ENTRY_HIT]" in text
    emu_ok = bool(re.search(r"\[JJFB_MRPGCMAP_ENTRY\][^\n]*result=EMU_OK", text))
    pxc_nz = bool(
        re.search(r"\[JJFB_P_FIELD_WRITE\][^\n]*off=0x0C[^\n]*new=0x[1-9A-Fa-f]", text)
        or re.search(r"\[JJFB_P_WRITE\][^\n]*off=0x[Cc][^\n]*new=0x[1-9A-Fa-f]", text)
    )
    if re.search(r"fault_during_entry|NEW_ENTRY_FAULT", text):
        return "NEW_ENTRY_FAULT"
    if entry_hit and emu_ok and not pxc_nz:
        if re.search(r"fault_addr=0x28|UC_MEM_READ_UNMAPPED|EXTCHUNK_FAULT", text):
            return "ENTRY_OK_BUT_NO_EXTCHUNK"
        return "ENTRY_OK_BUT_NO_EXTCHUNK"
    if re.search(r"unimplemented|UNIMPLEMENTED", text, re.I):
        return "NEW_API_MISSING"
    if re.search(r"\[JJFB_FILEOPEN_MISS\]|code=3006|FILEOPEN[^\n]*ok=0", text, re.I):
        return "NEW_FILE_MISS"
    if re.search(r"fault_addr=0x28|P\+0xC=0x0", text):
        return "NEW_ABI_FIELD_FAULT"
    if re.search(r"fault_pc=0x", text):
        return "NEW_ENTRY_FAULT"
    return "NONE"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("reports_dir")
    ap.add_argument("--wx-stdout", default="")
    args = ap.parse_args()
    reports = Path(args.reports_dir)
    reports.mkdir(parents=True, exist_ok=True)
    text = Path(args.stdout).read_text(encoding="utf-8", errors="replace") if Path(args.stdout).is_file() else ""

    entry_hit = yesno(text, "[JJFB_MRPGCMAP_ENTRY_HIT]")
    entry_ran = yesno(text, "[JJFB_MRPGCMAP_ENTRY]")
    emu_ok = "yes" if re.search(r"result=EMU_OK", text) else "no"
    pxc = "yes" if re.search(
        r"\[JJFB_P_FIELD_WRITE\][^\n]*off=0x0C[^\n]*new=0x[1-9A-Fa-f]", text
    ) or re.search(r"\[JJFB_P_WRITE\][^\n]*off=0x[Cc][^\n]*new=0x[1-9A-Fa-f]", text) else "no"
    wrote_c_summary = m1(text, r"wrote_C=(\S+)", "n/a")
    fault_pc = m1(text, r"fault_pc=(0x[0-9A-Fa-f]+)", "n/a")
    fault_addr = m1(text, r"fault_addr=(0x[0-9A-Fa-f]+)", "n/a")
    fault_class = classify_fault(text)
    order_entry_before_cont = "unknown"
    if "state=entry_called" in text and "state=callback_continuation" in text:
        i_entry = text.find("state=entry_called")
        i_cont = text.find("state=callback_continuation")
        order_entry_before_cont = "yes" if 0 <= i_entry < i_cont else "no"

    (reports / "phase6k_entry_order_change.md").write_text(
        "\n".join(
            [
                "# Phase 6K — entry order change",
                "",
                f"- entry_ran tag: `{entry_ran}`",
                f"- ENTRY_HIT: `{entry_hit}`",
                f"- emu OK: `{emu_ok}`",
                f"- entry_called before callback_continuation: `{order_entry_before_cont}`",
                "",
                "## Intended vs observed",
                "",
                f"- intended: image_base+8 (DOCUMENTED)",
                f"- live ENTRY_HIT / first meaningful entry: see stdout `[JJFB_MRPGCMAP_ENTRY_HIT]` / `[JJFB_6K_ENTRY_AUDIT]`",
                "",
            ]
        ),
        encoding="utf-8",
    )

    (reports / "phase6k_p_extchunk_publication.md").write_text(
        "\n".join(
            [
                "# Phase 6K — P+0xC publication after entry fix",
                "",
                f"- natural P+0xC write: `{pxc}`",
                f"- PUBLICATION wrote_C: `{wrote_c_summary}`",
                f"- fault_pc / fault_addr: `{fault_pc}` / `{fault_addr}`",
                "",
                "Forbidden: inventing P+0xC. Only guest-natural writes count.",
                "",
            ]
        ),
        encoding="utf-8",
    )

    (reports / "phase6k_next_fault_classification.md").write_text(
        "\n".join(
            [
                "# Phase 6K — next fault classification",
                "",
                f"- class: `{fault_class}`",
                f"- fault_pc: `{fault_pc}`",
                f"- fault_addr: `{fault_addr}`",
                "",
                "If NEW_ENTRY_FAULT during documented entry — do not skip; do not fake chunk.",
                "",
            ]
        ),
        encoding="utf-8",
    )

    wx = ""
    if args.wx_stdout and Path(args.wx_stdout).is_file():
        wx = Path(args.wx_stdout).read_text(encoding="utf-8", errors="replace")
    mid = entry_hit == "yes" and pxc == "yes"
    if mid and wx:
        wx_pxc = "yes" if re.search(r"wrote_C=yes|off=0x0C[^\n]*new=0x[1-9A-Fa-f]", wx) else "no"
        (reports / "phase6k_cross_target_after_entry_fix.md").write_text(
            "\n".join(
                [
                    "# Phase 6K — cross-target after entry fix",
                    "",
                    f"- jjfb natural P+0xC: `{pxc}`",
                    f"- wxjwq natural P+0xC: `{wx_pxc}`",
                    f"- jjfb fault: `{fault_pc}` / `{fault_addr}`",
                    f"- wxjwq fault_pc: `{m1(wx, r'fault_pc=(0x[0-9A-Fa-f]+)', 'n/a')}`",
                    "",
                ]
            ),
            encoding="utf-8",
        )
    else:
        (reports / "phase6k_cross_target_after_entry_fix.md").write_text(
            "\n".join(
                [
                    "# Phase 6K — cross-target after entry fix",
                    "",
                    "- status: `N/A` (6K-D only after mid success: ENTRY_HIT + natural P+0xC)",
                    f"- current ENTRY_HIT=`{entry_hit}` pxc=`{pxc}`",
                    "",
                ]
            ),
            encoding="utf-8",
        )

    print(f"entry_hit={entry_hit} pxc={pxc} fault_class={fault_class}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
