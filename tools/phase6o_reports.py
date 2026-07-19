#!/usr/bin/env python3
"""Phase 6O reports: post-extChunk ER_RW metadata bind."""
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
    ap.add_argument("--wx-stdout", default="")
    args = ap.parse_args()
    out = Path(args.stdout).read_text(errors="ignore")
    rdir = Path(args.report_dir)
    rdir.mkdir(parents=True, exist_ok=True)

    binds = grab(
        r"\[JJFB_ER_RW_BIND\]\s+module=(\S+)\s+module_id=(\d+)\s+P=0x([0-9A-Fa-f]+)\s+"
        r"p_base=0x([0-9A-Fa-f]+)\s+p_len=0x([0-9A-Fa-f]+)\s+"
        r"registry_base=0x([0-9A-Fa-f]+)\s+registry_len=0x([0-9A-Fa-f]+)\s+reason=(\S+)",
        out,
    )
    module_er = grab(
        r"\[MODULE_ER_RW\]\s+module=(gbrwcore\S*)\s+module_id=(\d+)\s+base=0x([0-9A-Fa-f]+)\s+"
        r"size=0x([0-9A-Fa-f]+)",
        out,
    )
    attempts = grab(r"\[JJFB_R9_SWITCH_ATTEMPT\].*", out)
    oks = grab(r"\[JJFB_R9_SWITCH_OK\].*", out)
    # Also count classic successful switch enter for gbrwcore
    classic_ok = grab(
        r"\[R9_SWITCH\]\s+stage=ENTER\s+from=\S+\s+to=gbrwcore\S*\s+.*new_r9=0x([0-9A-Fa-f]+)",
        out,
    )
    blocked = grab(r"\[R9_SWITCH_BLOCKED\]\s+reason=CALLEE_ER_RW_NOT_AVAILABLE\s+module=(gbrwcore\S*)", out)
    jjfb_blocked = grab(r"\[JJFB_R9_SWITCH_BLOCKED\].*", out)
    slot_calls = grab(r"\[JJFB_EXTCHUNK_SLOT_CALL\]", out)
    strcom = grab(
        r"\[JJFB_STRCOM\]|strcom_601=yes|strcom_800=yes|strcom_801=yes",
        out,
        re.I,
    )
    # High markers only — ignore warmup file opens / string export registration.
    gamelist = grab(
        r"gamelist started|export_called=yes|lib\.runapp.*called|mrc_loader\.ext loaded|"
        r"stage=gamelist_start",
        out,
        re.I,
    )
    summary = grab(r"\[JJFB_6O_SUMMARY\].*", out)
    mr_exit = bool(re.search(r"mythroad exit", out, re.I))
    pubs = grab(r"\[JJFB_EXTCHUNK_PUBLISH\].*new=0x(?!0+\b)[0-9A-Fa-f]+", out)

    gbrw_bound = any(int(b[3], 16) != 0 and int(b[5], 16) != 0 for b in binds) or any(
        int(m[2], 16) != 0 for m in module_er
    )
    r9_ok = bool(oks) or bool(classic_ok)
    # Early bootstrap block is expected; terminal if blocked AFTER bind with no OK
    bind_pos = out.find("[JJFB_ER_RW_BIND]")
    post_bind = out[bind_pos:] if bind_pos >= 0 else ""
    post_blocked = bool(
        re.search(r"\[R9_SWITCH_BLOCKED\]\s+reason=CALLEE_ER_RW_NOT_AVAILABLE\s+module=gbrwcore", post_bind)
    )
    terminal_blocked = post_blocked and not r9_ok

    high = bool(slot_calls) or bool(strcom) or bool(gamelist)
    mid = gbrw_bound and r9_ok and not terminal_blocked
    # Mid also prefers not immediate exit-only — note exit but don't hard-fail mid if R9_OK
    min_ok = gbrw_bound and not terminal_blocked

    faults = grab(
        r"UC_MEM_(?:READ|WRITE)_UNMAPPED|fault_addr=0x([0-9A-Fa-f]+)|mem_fault",
        out,
        re.I,
    )

    if high and mid:
        verdict, klass = "HIGH_SUCCESS", "SHELL_CONTINUATION_ADVANCED"
    elif mid:
        verdict, klass = "MID_SUCCESS", "ER_RW_BOUND_R9_SWITCH_OK"
    elif min_ok:
        verdict, klass = "MINIMUM_SUCCESS", "ER_RW_BOUND"
    elif binds or module_er:
        verdict, klass = "FAIL", "ER_RW_BIND_PARTIAL"
    else:
        verdict, klass = "FAIL", "ER_RW_BIND_MISSING"

    # Next fault classification
    fault_notes = []
    if re.search(r"UC_MEM_READ_UNMAPPED", out):
        fault_notes.append("UC_MEM_READ_UNMAPPED (classify; do not auto-fix)")
    if terminal_blocked:
        fault_notes.append("post-bind CALLEE_ER_RW_NOT_AVAILABLE still terminal")
    if mr_exit and not mid:
        fault_notes.append("mythroad exit without mid R9_OK")

    (rdir / "phase6o_er_rw_timeline.md").write_text(
        "# Phase 6O ER_RW Timeline\n\n"
        + "## JJFB_ER_RW_BIND\n\n"
        + (
            "\n".join(
                f"- module={m} id={i} P=0x{p} p_base=0x{pb} p_len=0x{pl} "
                f"registry=0x{rb}/0x{rl} reason={r}"
                for m, i, p, pb, pl, rb, rl, r in binds
            )
            or "- (none)"
        )
        + "\n\n## MODULE_ER_RW (gbrwcore)\n\n"
        + (
            "\n".join(f"- module={m} id={i} base=0x{b} size=0x{s}" for m, i, b, s in module_er)
            or "- (none)"
        )
        + "\n\n## Early BOOTSTRAP CALLEE_ER_RW_NOT_AVAILABLE (expected before P fill)\n\n"
        + f"- count={len(blocked)}\n"
        + "\n## Post-bind terminal blocked\n\n"
        + f"- `{terminal_blocked}`\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_registry_bind_result.md").write_text(
        f"# Phase 6O Registry Bind\n\nbound={gbrw_bound}\n\n"
        + (
            "\n".join(
                f"- {m} id={i} registry_base=0x{rb} registry_len=0x{rl} reason={r}"
                for m, i, _p, _pb, _pl, rb, rl, r in binds
            )
            or "- (no JJFB_ER_RW_BIND)"
        )
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_r9_switch_result.md").write_text(
        "# Phase 6O R9 Switch\n\n"
        + f"- attempts={len(attempts)}\n- JJFB_R9_SWITCH_OK={len(oks)}\n"
        + f"- classic ENTER new_r9 for gbrwcore={len(classic_ok)}\n"
        + f"- early CALLEE_ER_RW_NOT_AVAILABLE={len(blocked)}\n"
        + f"- JJFB deferred blocked={len(jjfb_blocked)}\n\n"
        + "## Attempts\n\n"
        + ("\n".join(f"- `{a}`" for a in attempts) or "- (none)")
        + "\n\n## OK\n\n"
        + ("\n".join(f"- `{a}`" for a in oks) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_post_entry_continuation.md").write_text(
        f"# Phase 6O Post-Entry Continuation\n\n"
        f"- extChunk PUBLISH nonzero seen: {bool(pubs)}\n"
        f"- mythroad exit: {mr_exit}\n"
        f"- mid R9 path: {mid}\n"
        f"- 6O summary: {summary[0] if summary else '(none)'}\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_slot_trigger_result.md").write_text(
        f"# Phase 6O Slot Trigger\n\n"
        f"- SLOT_CALL count: {len(slot_calls)}\n"
        f"- action: {'STOP for slot API phase' if slot_calls else 'no slot call — do not expand matrix'}\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_next_fault_classification.md").write_text(
        "# Phase 6O Next Fault Classification\n\n"
        + ("\n".join(f"- {n}" for n in fault_notes) or "- (no new classified blocker beyond exit/observe)")
        + f"\n\n- raw fault-ish hits: {len(faults)}\n",
        encoding="utf-8",
    )

    (rdir / "phase6o_verdict.md").write_text(
        f"""# Phase 6O Verdict

- **verdict:** `{verdict}`
- **class:** `{klass}`
- **gbrwcore ER_RW bound:** `{gbrw_bound}`
- **R9_SWITCH_OK / ENTER:** `{r9_ok}`
- **post-bind terminal CALLEE_ER_RW_NOT_AVAILABLE:** `{terminal_blocked}`
- **SLOT_CALL:** `{len(slot_calls)}`
- **mythroad exit:** `{mr_exit}`

## Evidence

- P+0/+4 layout: **DOCUMENTED**
- Bind reasons: `mr_c_function_st_metadata_bind` / `platform_er_rw_publication_restore`
- Early BOOTSTRAP block before P fill remains **DOCUMENTED** order (not a failure by itself)

## 6O summary

"""
        + ("\n".join(f"- `{s}`" for s in summary) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    conclusion = f"""# CONCLUSION — Phase 6O Post-ExtChunk ER_RW Bind

**Verdict:** {verdict} (`{klass}`)

| Gate | Result |
|---|---|
| Minimum: gbrwcore registry ER_RW nonzero | {"PASS" if gbrw_bound else "FAIL"} |
| Post-bind CALLEE_ER_RW_NOT_AVAILABLE not terminal | {"PASS" if not terminal_blocked else "FAIL"} |
| Mid: R9_SWITCH_OK | {"PASS" if r9_ok else "FAIL"} |
| High: SLOT_CALL / strCom / gamelist | {"PASS" if high else "observe-only (none)"} |

## Next

"""
    if high:
        conclusion += "- High markers observed — schedule slot/strCom/loader follow-up; do not expand matrix inside 6O.\n"
    elif mid:
        conclusion += "- Mid success: continue observe shell native path; do not fake slot APIs yet.\n"
    elif min_ok:
        conclusion += "- Registry bound but R9_OK missing — check deferred switch / later GCO BLX.\n"
    else:
        conclusion += "- Bind missing — check P+0/+4 watch, provider P ownership, JJFB_ER_RW_BIND_RESTORE.\n"

    (rdir / "CONCLUSION.md").write_text(conclusion, encoding="utf-8")
    (rdir / "phase6o_conclusion.md").write_text(conclusion, encoding="utf-8")

    if args.wx_stdout and Path(args.wx_stdout).is_file():
        wx = Path(args.wx_stdout).read_text(errors="ignore")
        wx_b = grab(r"\[JJFB_ER_RW_BIND\].*", wx)
        (rdir / "phase6o_wxjwq.md").write_text(
            "# Phase 6O wxjwq\n\n" + ("\n".join(f"- `{l}`" for l in wx_b) or "- (no bind)") + "\n",
            encoding="utf-8",
        )

    print(f"phase6o_reports: verdict={verdict} class={klass} bound={gbrw_bound} r9_ok={r9_ok}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
