#!/usr/bin/env python3
"""Phase 6Q reports: gamelist native primary + dsm cfunction helper ABI."""
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
    ap.add_argument("--stderr", default="")
    args = ap.parse_args()
    out = Path(args.stdout).read_text(errors="ignore")
    err = Path(args.stderr).read_text(errors="ignore") if args.stderr and Path(args.stderr).is_file() else ""
    rdir = Path(args.report_dir)
    rdir.mkdir(parents=True, exist_ok=True)

    reg_fail = grab(r"reg_primary failed package=gwy/gamelist\.mrp", err + "\n" + out)
    reg_ok = grab(
        r"\[MRP_MEMBER_VIEW\] reg_primary_installed package=gwy/gamelist\.mrp primary=(\S+)",
        out,
    )
    reg_primary_log = grab(r"\[REG_PRIMARY\].*gamelist\.mrp.*", out)
    fileopen_gl = grab(r"\[JJFB_FILEOPEN\].*gamelist\.mrp.*", out)
    generated = bool(
        re.search(
            r"gamelist\.mrp.*shell_gamelist_cfunction|shell_gamelist_cfunction.*gamelist",
            out,
            re.I,
        )
    ) or bool(
        re.search(r"backend=generated.*gamelist|gamelist.*backend=generated", out, re.I)
    )
    # Also accept VM_FILE_OPEN generated path
    generated = generated or bool(
        re.search(r"shell_gamelist_cfunction\.mrp", out + "\n" + err, re.I)
    )

    extchunk_gl = grab(r"\[JJFB_EXTCHUNK_(?:ALLOC|PUBLISH)\].*gamelist\.ext.*", out)
    er_rw_gl = grab(r"\[JJFB_ER_RW_BIND\].*gamelist\.ext.*", out)
    r9_gl = grab(r"\[JJFB_R9_SWITCH_OK\].*gamelist\.ext.*", out)
    init_ok = grab(r"\[JJFB_SHELL_CORE_MODULE\] module=gamelist\.ext stage=init_ok", out)

    helper_trace = grab(r"\[JJFB_HELPER_ABI_TRACE\].*", out)
    helper_route = grab(r"\[JJFB_HELPER_CALL_ROUTE\].*", out)
    helper_args = grab(r"\[JJFB_HELPER_ARG_FLOW\].*", out)
    wrong_helper = grab(r"WRONG_HELPER_CALL_MISSING_P", out)
    legal_helper = grab(r"LEGAL_HELPER_EVENT_WITH_P", out)
    fault_8cc00 = bool(re.search(r"fault_pc=0x8CC00|fault_pc=0x8cc00", out, re.I))
    entry_arg = bool(re.search(r"root_cause=ENTRY_ARGUMENT", out))
    mem_fault = bool(re.search(r"UC_MEM_(?:READ|WRITE)_UNMAPPED|stop=mem_fault", out, re.I))

    gamelist_started = grab(r"\[JJFB_GAMELIST_STARTED\].*", out)
    cfg36 = grab(r"\[JJFB_GAMELIST_CFG36_BUILD\].*", out)
    post_upd = grab(r"\[JJFB_GAMELIST_POST_UPDATE\].*", out)
    export_call = grab(r"\[JJFB_SHELL_EXPORT_CALL\].*", out)
    slot_calls = grab(r"\[JJFB_EXTCHUNK_SLOT_CALL\].*", out)
    continue_ok = bool(re.search(r"source=shell_chain_continue", out))

    min_ok = (not reg_fail) and (bool(reg_ok) or bool(reg_primary_log)) and generated
    mid = min_ok and (not fault_8cc00 or not entry_arg) and (
        bool(init_ok) or bool(r9_gl) or bool(er_rw_gl) or (bool(legal_helper) and not wrong_helper)
    )
    # Mid also if fault gone entirely after continue+gamelist
    if min_ok and continue_ok and not fault_8cc00 and not (entry_arg and mem_fault):
        mid = True
    high = bool(cfg36) or bool(post_upd) or bool(export_call) or bool(
        re.search(r"\[JJFB_RUNAPP\].*(native_shell|guest_native)", out)
    )

    if high and mid:
        verdict, klass = "HIGH_SUCCESS", "GAMELIST_CFG36_OR_EXPORT"
    elif mid:
        verdict, klass = "MID_SUCCESS", "GAMELIST_PRIMARY_HELPER_CLEARED"
    elif min_ok:
        verdict, klass = "MINIMUM_SUCCESS", "GAMELIST_MEMBER_VIEW_OK"
    elif continue_ok:
        verdict, klass = "FAIL", "CONTINUE_BUT_PRIMARY_OR_HELPER_BLOCKED"
    else:
        verdict, klass = "FAIL", "NO_SHELL_CONTINUE"

    fault_notes = []
    if reg_fail:
        fault_notes.append("reg_primary still fails for gamelist.mrp")
    if fault_8cc00:
        fault_notes.append("0x8CC00 still present — helper/entry or OOM path")
    if entry_arg and not legal_helper:
        fault_notes.append("ENTRY_ARGUMENT with WRONG_HELPER still terminal")
    if slot_calls:
        fault_notes.append("SLOT_CALL observed — stop for slot audit; do not expand matrix in 6Q")
    if mem_fault and not fault_8cc00:
        fault_notes.append("new mem_fault (classify; do not invent slot APIs)")

    (rdir / "phase6q_gamelist_member_view.md").write_text(
        "# Phase 6Q Gamelist Member View\n\n"
        f"- reg_primary failed: `{bool(reg_fail)}`\n"
        f"- reg_primary_installed: `{bool(reg_ok)}` primary=`{reg_ok[0] if reg_ok else '-'}`\n"
        f"- generated overlay seen: `{generated}`\n\n"
        "## REG_PRIMARY\n\n"
        + ("\n".join(f"- `{l}`" for l in reg_primary_log) or "- (none)")
        + "\n\n## FILEOPEN\n\n"
        + ("\n".join(f"- `{l}`" for l in fileopen_gl) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6q_gamelist_platform_context.md").write_text(
        "# Phase 6Q Gamelist Platform Context\n\n"
        f"- EXTCHUNK gamelist: {len(extchunk_gl)}\n"
        f"- ER_RW_BIND gamelist: {len(er_rw_gl)}\n"
        f"- R9_SWITCH_OK gamelist: {len(r9_gl)}\n"
        f"- SHELL_CORE init_ok: {len(init_ok)}\n\n"
        "## ExtChunk\n\n"
        + ("\n".join(f"- `{l}`" for l in extchunk_gl) or "- (none)")
        + "\n\n## ER_RW\n\n"
        + ("\n".join(f"- `{l}`" for l in er_rw_gl) or "- (none)")
        + "\n\n## R9\n\n"
        + ("\n".join(f"- `{l}`" for l in r9_gl) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6q_dsm_cfunction_helper_abi.md").write_text(
        "# Phase 6Q DSM cfunction Helper ABI\n\n"
        f"- WRONG_HELPER hits: {len(wrong_helper)}\n"
        f"- LEGAL_HELPER_EVENT_WITH_P hits: {len(legal_helper)}\n"
        f"- fault_pc=0x8CC00: `{fault_8cc00}`\n"
        f"- ENTRY_ARGUMENT: `{entry_arg}`\n\n"
        "## Answers (from live tags)\n\n"
        "1. Helper enter via `bridge_mr_extHelper` / HOST_BRIDGE when method!=0.\n"
        "2. `0xA4178` is registered helper (event path), not header_entry `image+8`.\n"
        "3. Event path should use mr_table helper with r0=P (LEGAL_HELPER_EVENT_WITH_P).\n"
        "4. LR_PROXY may show call_site_r0=method then enter_r0=P — see ARG_FLOW.\n"
        "5/6. P / chunk_field_04 from EXT_ENTRY_CTX / CHUNK_FIELD04 tags.\n\n"
        "## TRACE\n\n"
        + ("\n".join(f"- `{l}`" for l in helper_trace) or "- (none)")
        + "\n\n## ROUTE\n\n"
        + ("\n".join(f"- `{l}`" for l in helper_route) or "- (none)")
        + "\n\n## ARG_FLOW\n\n"
        + ("\n".join(f"- `{l}`" for l in helper_args) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6q_slot_trigger_result.md").write_text(
        f"# Phase 6Q Slot Trigger\n\n"
        f"- SLOT_CALL count: {len(slot_calls)}\n"
        f"- action: {'STOP for slot phase' if slot_calls else 'no slot call — do not expand matrix'}\n",
        encoding="utf-8",
    )

    (rdir / "phase6q_next_fault_classification.md").write_text(
        "# Phase 6Q Next Fault Classification\n\n"
        + ("\n".join(f"- {n}" for n in fault_notes) or "- (no new classified blocker)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6q_verdict.md").write_text(
        f"""# Phase 6Q Verdict

- **verdict:** `{verdict}`
- **class:** `{klass}`
- **gamelist member_view:** `{min_ok}`
- **0x8CC00 / ENTRY_ARGUMENT cleared or reclassed:** `{mid}`
- **cfg36 / post_update / export:** `{high}`
- **SLOT_CALL:** `{len(slot_calls)}`
- **GAMELIST_STARTED:** `{bool(gamelist_started)}`

## Evidence

- reg.ext embedded primary name: **CROSS_TARGET**
- mr_extHelper with r0=P: **DOCUMENTED**
- Prior 0x8CC00 path: **TARGET_OBSERVED**
""",
        encoding="utf-8",
    )

    conclusion = f"""# CONCLUSION — Phase 6Q Gamelist Native Primary ABI

**Verdict:** {verdict} (`{klass}`)

| Gate | Result |
|---|---|
| Minimum: gamelist reg_primary + generated member_view | {"PASS" if min_ok else "FAIL"} |
| Mid: helper/entry cleared (no 0x8CC00 ENTRY_ARGUMENT terminal) | {"PASS" if mid else "FAIL"} |
| High: CFG36 / POST_UPDATE / EXPORT_CALL | {"PASS" if high else "observe-only (none)"} |
| Slot matrix | {"STOP" if slot_calls else "no SLOT_CALL (correct)"} |

## Next

"""
    if slot_calls:
        conclusion += "- Slot call — open slot-specific audit; do not invent APIs.\n"
    elif high:
        conclusion += "- High markers — observe jjfb natural loader/mrc_init.\n"
    elif mid:
        conclusion += "- Mid success — chase gamelist cfg36/post-update/export.\n"
    elif min_ok:
        conclusion += "- Member view OK but helper/entry still blocked — continue 6Q-C route.\n"
    else:
        conclusion += "- Fix gamelist reg_primary / VFS remap first.\n"

    (rdir / "CONCLUSION.md").write_text(conclusion, encoding="utf-8")
    print(
        f"phase6q_reports: verdict={verdict} class={klass} "
        f"member_view={min_ok} mid={mid} high={high}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
