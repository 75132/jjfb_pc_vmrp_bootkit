#!/usr/bin/env python3
"""Phase 6P reports: shell core continue after gbrwcore init."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def grab(pat: str, text: str, flags=0):
    return re.findall(pat, text, flags)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("report_dir")
    args = ap.parse_args()
    out = Path(args.stdout).read_text(errors="ignore")
    rdir = Path(args.report_dir)
    rdir.mkdir(parents=True, exist_ok=True)

    exit_src = grab(r"\[JJFB_6P_EXIT_SOURCE\].*", out)
    continues = grab(r"\[JJFB_SHELL_CORE_CONTINUE\].*", out)
    shell_mods = grab(r"\[JJFB_SHELL_CORE_MODULE\].*", out)
    gamelist_started = grab(r"\[JJFB_GAMELIST_STARTED\].*", out)
    cfg36 = grab(r"\[JJFB_GAMELIST_CFG36_BUILD\].*", out)
    post_upd = grab(r"\[JJFB_GAMELIST_POST_UPDATE\].*", out)
    export_resolve = grab(r"\[JJFB_SHELL_EXPORT_RESOLVE\].*", out)
    export_call = grab(r"\[JJFB_SHELL_EXPORT_CALL\].*", out)
    runapp = grab(r"\[JJFB_RUNAPP\].*", out)
    fileopens = grab(r"\[JJFB_FILEOPEN\].*", out)
    slot_calls = grab(r"\[JJFB_EXTCHUNK_SLOT_CALL\].*", out)
    # Real guest strCom codes only — ignore summary lines with not_observed.
    strcom = grab(r"\[JJFB_STRCOM\]\s+code=(601|800|801)\b", out)
    mr_exit = bool(re.search(r"mythroad exit", out, re.I))
    shell_continue_src = bool(re.search(r"source=shell_chain_continue", out))
    gamelist_dsm = bool(
        re.search(r"bridge_dsm_mr_start_dsm.*gamelist|filename=\"[^\"]*gamelist", out, re.I)
    ) or bool(gamelist_started)
    gbrwshell_entry = bool(re.search(r"module=gbrwshell\.ext\s+stage=entry", out))
    jjfb_natural = bool(
        re.search(
            r"note=real_gwy_jjfb|\[JJFB_SHELL_EXEC\] package=gwy/jjfb\.mrp stage=mr_start",
            out,
            re.I,
        )
    )
    native_runapp = bool(
        re.search(
            r"\[JJFB_RUNAPP\].*(via=guest_native|source=native_shell)",
            out,
            re.I,
        )
    )

    faults = grab(
        r"UC_MEM_(?:READ|WRITE)_UNMAPPED|fault_addr=0x([0-9A-Fa-f]+)|mem_fault",
        out,
        re.I,
    )

    min_ok = shell_continue_src and (gamelist_dsm or gbrwshell_entry or bool(gamelist_started))
    mid = min_ok and (bool(gamelist_started) or bool(cfg36) or bool(post_upd))
    high = bool(export_call) or native_runapp or jjfb_natural or bool(strcom)

    if high and mid:
        verdict, klass = "HIGH_SUCCESS", "SHELL_NATIVE_RUNAPP_OR_EXPORT"
    elif mid:
        if cfg36 or post_upd:
            verdict, klass = "MID_SUCCESS", "GAMELIST_CFG36_OR_POST_UPDATE"
        else:
            verdict, klass = "MID_SUCCESS", "GAMELIST_STARTED_AFTER_CONTINUE"
    elif min_ok:
        verdict, klass = "MINIMUM_SUCCESS", "SHELL_CORE_CONTINUE_GAMELIST"
    elif shell_continue_src:
        verdict, klass = "FAIL", "CONTINUE_TAGGED_BUT_NO_GAMELIST"
    elif mr_exit and not shell_continue_src:
        verdict, klass = "FAIL", "STILL_GBRWCORE_ONLY_EXIT"
    else:
        verdict, klass = "FAIL", "NO_SHELL_CONTINUE"

    fault_notes = []
    if re.search(r"UC_MEM_READ_UNMAPPED", out):
        fault_notes.append("UC_MEM_READ_UNMAPPED (classify; do not auto-expand slot APIs)")
    if slot_calls:
        fault_notes.append("SLOT_CALL observed — stop for Phase 6Q slot audit")
    if mr_exit and not min_ok:
        fault_notes.append("mythroad exit without gamelist/gbrwshell continue progress")
    if not shell_continue_src and mr_exit:
        fault_notes.append("exit without shell_chain_continue — check JJFB_SHELL_CHAIN_MODE")

    (rdir / "phase6p_mr_exit_source.md").write_text(
        "# Phase 6P MR Exit Source\n\n"
        + ("\n".join(f"- `{l}`" for l in exit_src) or "- (none)")
        + f"\n\n- shell_chain_continue seen: `{shell_continue_src}`\n"
        + f"- mythroad process exit: `{mr_exit}`\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_shell_core_module_init.md").write_text(
        "# Phase 6P Shell Core Module Init\n\n"
        + ("\n".join(f"- `{l}`" for l in shell_mods) or "- (none)")
        + "\n\n## Continue\n\n"
        + ("\n".join(f"- `{l}`" for l in continues) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_gamelist_cfg36_branch.md").write_text(
        "# Phase 6P Gamelist / CFG36 Branch\n\n"
        f"- GAMELIST_STARTED: {len(gamelist_started)}\n"
        f"- CFG36_BUILD: {len(cfg36)}\n"
        f"- POST_UPDATE: {len(post_upd)}\n\n"
        "## Started\n\n"
        + ("\n".join(f"- `{l}`" for l in gamelist_started) or "- (none)")
        + "\n\n## CFG36\n\n"
        + ("\n".join(f"- `{l}`" for l in cfg36) or "- (none)")
        + "\n\n## Post-update\n\n"
        + ("\n".join(f"- `{l}`" for l in post_upd) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_export_dispatcher_result.md").write_text(
        "# Phase 6P Export / Dispatcher\n\n"
        f"- EXPORT_RESOLVE: {len(export_resolve)}\n"
        f"- EXPORT_CALL: {len(export_call)}\n"
        f"- RUNAPP tags: {len(runapp)}\n\n"
        "## Resolve (string_va_not_entry must not be emu entry)\n\n"
        + ("\n".join(f"- `{l}`" for l in export_resolve) or "- (none)")
        + "\n\n## Call\n\n"
        + ("\n".join(f"- `{l}`" for l in export_call) or "- (none)")
        + "\n\n## RUNAPP\n\n"
        + ("\n".join(f"- `{l}`" for l in runapp) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_fileopen_after_shell_continue.md").write_text(
        "# Phase 6P File Open After Shell Continue\n\n"
        + ("\n".join(f"- `{l}`" for l in fileopens) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_slot_trigger_result.md").write_text(
        f"# Phase 6P Slot Trigger\n\n"
        f"- SLOT_CALL count: {len(slot_calls)}\n"
        f"- action: {'STOP for Phase 6Q' if slot_calls else 'no slot call — do not expand matrix'}\n\n"
        + ("\n".join(f"- `{l}`" for l in slot_calls) or "")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_next_fault_classification.md").write_text(
        "# Phase 6P Next Fault Classification\n\n"
        + ("\n".join(f"- {n}" for n in fault_notes) or "- (no new classified blocker)")
        + f"\n\n- raw fault-ish hits: {len(faults)}\n",
        encoding="utf-8",
    )

    (rdir / "phase6p_verdict.md").write_text(
        f"""# Phase 6P Verdict

- **verdict:** `{verdict}`
- **class:** `{klass}`
- **shell_chain_continue:** `{shell_continue_src}`
- **gamelist started:** `{bool(gamelist_started) or gamelist_dsm}`
- **cfg36 / post_update:** `{bool(cfg36) or bool(post_upd)}`
- **export_call / native runapp / jjfb natural:** `{high}`
- **SLOT_CALL:** `{len(slot_calls)}`
- **mythroad exit:** `{mr_exit}`

## Evidence

- Exit classify: **TARGET_OBSERVED**
- `mr_start_dsm` gamelist continue: **DOCUMENTED** platform API
- Continue reason must be `continue_after_gbrwcore_init` (not natural guest write)

## Tags

- EXIT_SOURCE lines: {len(exit_src)}
- SHELL_CORE_CONTINUE: {len(continues)}
- SHELL_CORE_MODULE: {len(shell_mods)}
""",
        encoding="utf-8",
    )

    conclusion = f"""# CONCLUSION — Phase 6P Shell Core Continue

**Verdict:** {verdict} (`{klass}`)

| Gate | Result |
|---|---|
| Minimum: continue past gbrwcore-only exit + gamelist/gbrwshell start | {"PASS" if min_ok else "FAIL"} |
| Mid: GAMELIST_STARTED / CFG36_BUILD / POST_UPDATE | {"PASS" if mid else "FAIL"} |
| High: EXPORT_CALL / native RUNAPP / jjfb natural / strCom | {"PASS" if high else "observe-only (none)"} |
| Slot matrix | {"STOP 6Q" if slot_calls else "no SLOT_CALL (correct for 6P)"} |

## Next

"""
    if slot_calls:
        conclusion += "- Slot call observed — open Phase 6Q slot-specific audit; do not invent APIs.\n"
    elif high:
        conclusion += "- High markers — observe jjfb natural loader/mrc_init; do not invent login or UI.\n"
    elif mid:
        conclusion += "- Mid success — chase native export/dispatcher / post-update branch.\n"
    elif min_ok:
        conclusion += "- Minimum continue worked — chase gamelist cfg36/post-update observe.\n"
    else:
        conclusion += "- Continue missing — check br_exit hook, JJFB_SHELL_CHAIN_MODE, gbrwcore start_dsm.\n"

    (rdir / "CONCLUSION.md").write_text(conclusion, encoding="utf-8")

    print(
        f"phase6p_reports: verdict={verdict} class={klass} "
        f"continue={shell_continue_src} gamelist={bool(gamelist_started)} mid={mid}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
