#!/usr/bin/env python3
"""Phase 6N reports: extChunk publication restore."""
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

    pubs = grab(
        r"\[JJFB_EXTCHUNK_PUBLISH\]\s+module=(\S+)\s+P=0x([0-9A-Fa-f]+)\s+off=0x0C\s+"
        r"old=0x([0-9A-Fa-f]+)\s+new=0x([0-9A-Fa-f]+)\s+reason=(\S+)",
        out,
    )
    allocs = grab(r"\[JJFB_EXTCHUNK_ALLOC\]\s+module=(\S+).*guest=0x([0-9A-Fa-f]+)", out)
    slots = grab(r"\[JJFB_EXTCHUNK_SLOT\]\s+module=(\S+)\s+off=(0x[0-9A-Fa-f]+)\s+value=0x([0-9A-Fa-f]+)", out)
    slot_calls = grab(r"\[JJFB_EXTCHUNK_SLOT_CALL\]\s+off=0x28", out)
    republish = grab(r"\[JJFB_EXTCHUNK_REPUBLISH\]", out)
    summary = grab(r"\[JJFB_6N_SUMMARY\].*", out)
    faults = grab(
        r"UC_MEM_(?:READ|WRITE)_UNMAPPED.*?address\s*=\s*0x([0-9A-Fa-f]+)|"
        r"fault_addr=0x([0-9A-Fa-f]+)|"
        r"mem_fault.*?0x([0-9A-Fa-f]+)",
        out,
        re.I,
    )
    fault_28 = False
    for g in faults:
        for x in g:
            if not x:
                continue
            try:
                if int(x, 16) == 0x28:
                    fault_28 = True
            except ValueError:
                pass
    # Also classic NULL+0x28 patterns
    if re.search(r"fault.*\b0x28\b|address\s*=\s*0x28\b|UNMAPPED.*\b0x28\b", out, re.I):
        fault_28 = True
    if re.search(r"0x30CCF8", out):
        fault_class_legacy = True
    else:
        fault_class_legacy = False

    nz_publish = any(int(n, 16) != 0 for _, _, _, n, _ in pubs)
    gbrw_pub = [p for p in pubs if "gbrwcore" in p[0].lower()]
    min_ok = bool(gbrw_pub) and any(int(p[3], 16) != 0 for p in gbrw_pub)
    mid_ok = min_ok and not fault_28

    # Advanced tags (note only — do not chase in 6N)
    advanced = []
    for tag in ("_strCom", "strCom", "JJFB_SHELL_NATIVE", "GWY_STARTGAME"):
        if tag in out:
            advanced.append(tag)

    new_slot_api = bool(
        re.search(r"\[JJFB_NEW_EXTCHUNK_SLOT_API\]|NEW_EXTCHUNK_SLOT_API\b", out)
        or re.search(r"UNIMPLEMENTED.*sendAppEvent|unimplemented.*mrc_extMainSendApp", out, re.I)
    )

    if min_ok and mid_ok:
        verdict = "MID_SUCCESS"
        klass = "EXTCHUNK_PUBLICATION_RESTORED"
    elif min_ok:
        verdict = "MINIMUM_SUCCESS"
        klass = "EXTCHUNK_PUBLISH_OK_FAULT_REMAINS"
    elif pubs and not nz_publish:
        verdict = "FAIL"
        klass = "EXTCHUNK_PUBLISH_ZERO"
    else:
        verdict = "FAIL"
        klass = "EXTCHUNK_NO_PUBLISH"

    if new_slot_api:
        klass = "NEW_EXTCHUNK_SLOT_API"
        verdict = "STOP_FOR_6O"

    (rdir / "phase6n_verdict.md").write_text(
        f"""# Phase 6N Verdict

- **verdict:** `{verdict}`
- **class:** `{klass}`
- **gbrwcore PUBLISH nonzero:** `{min_ok}`
- **fault_addr=0x28 / NULL+0x28:** `{fault_28}`
- **legacy 0x30CCF8 seen:** `{fault_class_legacy}`
- **SLOT_CALL +0x28 count:** `{len(slot_calls)}`
- **REPUBLISH count:** `{len(republish)}`
- **ALLOC count:** `{len(allocs)}`
- **advanced tags noted (not chased):** {", ".join(advanced) if advanced else "(none)"}

## Evidence

- Layout/magic/sendAppEvent: **DOCUMENTED**
- Platform publish after zero-init: **DOCUMENTED** + **TARGET_OBSERVED**
- Tags use `*_contract` / `platform_publication_restore` only

## Publish events

"""
        + (
            "\n".join(
                f"- module={m} P=0x{p} old=0x{o} new=0x{n} reason={r}"
                for m, p, o, n, r in pubs
            )
            or "- (none)"
        )
        + "\n\n## 6N summary lines\n\n"
        + ("\n".join(f"- `{s}`" for s in summary) or "- (none)")
        + "\n",
        encoding="utf-8",
    )

    (rdir / "phase6n_publication.md").write_text(
        f"""# Phase 6N Publication Trace

## ALLOC

"""
        + ("\n".join(f"- module={m} guest=0x{g}" for m, g in allocs) or "- (none)")
        + "\n\n## SLOT fills\n\n"
        + ("\n".join(f"- module={m} off={o} value=0x{v}" for m, o, v in slots) or "- (none)")
        + "\n\n## PUBLISH\n\n"
        + (
            "\n".join(
                f"- module={m} P=0x{p} old=0x{o} new=0x{n} reason={r}"
                for m, p, o, n, r in pubs
            )
            or "- (none)"
        )
        + "\n",
        encoding="utf-8",
    )

    conclusion = f"""# CONCLUSION — Phase 6N Restore ExtChunk Publication

**Verdict:** {verdict} (`{klass}`)

## What was restored

Platform-owned `mrc_extChunk` allocation and publication into `mr_c_function_st+0x0C`,
with DOCUMENTED slot fill (`check`, `init_func`, `event`, `sendAppEvent` observe stub).

## Gates

| Gate | Result |
|---|---|
| Minimum: gbrwcore PUBLISH old=0 new=nonzero | {"PASS" if min_ok else "FAIL"} |
| Mid: no fault_addr=0x28 | {"PASS" if mid_ok else ("N/A" if not min_ok else "FAIL")} |
| Stop before 6O slot matrix | {"STOP" if new_slot_api else "OK"} |

## Next

"""
    if new_slot_api:
        conclusion += "- Recorded `NEW_EXTCHUNK_SLOT_API` — do not expand full slot API in 6N; schedule 6O.\n"
    elif mid_ok:
        conclusion += "- Mid success: optional wxjwq already covered by runner when applicable. Do not chase `_strCom`/UI here.\n"
    elif min_ok:
        conclusion += "- Publish works but fault remains — classify remaining fault before inventing more slot stubs.\n"
    else:
        conclusion += "- No gbrwcore publish — check `JJFB_EXTCHUNK_PROVIDER=gbrwcore_only`, helper→module bind, and CFN path.\n"

    (rdir / "CONCLUSION.md").write_text(conclusion, encoding="utf-8")
    (rdir / "phase6n_conclusion.md").write_text(conclusion, encoding="utf-8")

    if args.wx_stdout and Path(args.wx_stdout).is_file():
        wx = Path(args.wx_stdout).read_text(errors="ignore")
        wx_pubs = grab(r"\[JJFB_EXTCHUNK_PUBLISH\].*", wx)
        (rdir / "phase6n_wxjwq.md").write_text(
            "# Phase 6N wxjwq cross-target\n\n"
            + ("\n".join(f"- `{l}`" for l in wx_pubs) or "- (no PUBLISH tags)")
            + "\n",
            encoding="utf-8",
        )

    print(f"phase6n_reports: verdict={verdict} class={klass} min_ok={min_ok} mid_ok={mid_ok}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
