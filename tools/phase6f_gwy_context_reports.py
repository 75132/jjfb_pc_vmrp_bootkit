#!/usr/bin/env python3
"""Phase 6F: aggregate live stdout + static xref into required markdown reports."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


def m1(text: str, pat: str, default: str = "") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def main() -> int:
    if len(sys.argv) < 4:
        print(
            "usage: phase6f_gwy_context_reports.py <stdout> <reports_dir> <gwy_root> [txt_report]",
            file=sys.stderr,
        )
        return 2
    stdout_path = Path(sys.argv[1])
    reports = Path(sys.argv[2])
    gwy_root = Path(sys.argv[3])
    reports.mkdir(parents=True, exist_ok=True)
    text = stdout_path.read_text(encoding="utf-8", errors="replace") if stdout_path.exists() else ""

    # Run static xref into chain md
    xref = Path(__file__).resolve().parent / "phase6f_gwy_shell_xref.py"
    chain_md = reports / "phase6f_gwy_startgame_runapp_chain.md"
    xref_json = reports / "phase6f_shell_xref.json"
    subprocess.run(
        [sys.executable, str(xref), str(gwy_root), "--json", str(xref_json), "--md", str(chain_md)],
        check=False,
    )

    # cfg36
    cfg = gwy_root / "cfg.bin"
    cfg_md = reports / "phase6f_cfg36_contract.md"
    cfg_lines = [
        "# Phase 6F — cfg index=36 contract",
        "",
        "## Expected (docs/05_LAUNCH_CONTRACT.md) — DOCUMENTED",
        "",
        "```",
        "napptype=12 nextid=482 ncode=512 narg=0 narg1=1",
        "nmrpname=gwy/jjfb.mrp flag=gwyblink",
        "```",
        "",
        "## Live launch param",
        "",
        f"`{m1(text, r'\[JJFB_LAUNCH_CONTEXT\][^\n]*param=\"([^\"]*)\"', '?')}`",
        "",
        f"match: `{m1(text, r'\[JJFB_CFG36_CONTRACT\][^\n]*match=(\S+)', '?')}`",
        "",
        f"consumer: `{m1(text, r'\[JJFB_CFG36_CONTRACT\][^\n]*consumer=(\S+)', '?')}`",
        "",
    ]
    if cfg.exists():
        try:
            import json

            out = subprocess.check_output(
                [
                    sys.executable,
                    str(Path(__file__).resolve().parent / "gwy_cfg_inspect.py"),
                    str(cfg),
                    "--index",
                    "36",
                ],
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            cfg_lines += ["## cfg.bin inspect", "", "```json", out.strip()[:4000], "```", ""]
        except Exception as e:
            cfg_lines += [f"_cfg inspect failed: {e}_", ""]
    cfg_md.write_text("\n".join(cfg_lines) + "\n", encoding="utf-8")

    # context map
    klass = m1(text, r"\[JJFB_GWY_CONTEXT_SUMMARY\][^\n]*gwy_context_class=(\S+)", "UNKNOWN")
    nxt = m1(text, r"\[JJFB_GWY_CONTEXT_SUMMARY\][^\n]*next_allowed_fix=(\S+)", "NONE")
    bypass = m1(text, r"\[JJFB_GWY_CONTEXT_SUMMARY\][^\n]*shell_bypassed=(\S+)", "?")
    writes = m1(text, r"\[JJFB_GWY_CONTEXT_SUMMARY\][^\n]*pxc_writes_seen=(\d+)", "0")
    map_md = reports / "phase6f_p_extchunk_context_map.md"
    map_md.write_text(
        "\n".join(
            [
                "# Phase 6F — P.mrc_extChunk ↔ GWY context map",
                "",
                f"- live class: `{klass}`",
                f"- shell_bypassed: `{bypass}`",
                f"- pxc_writes_seen: `{writes}`",
                f"- next_allowed_fix: `{nxt}`",
                "",
                "| candidate provider | module | reason | evidence | executed_in_current_launch? | writes_P+0xC? | missing precondition |",
                "|---|---|---|---|---|---|---|",
                "| gbrwcore startGame/runapp | gbrwcore.mrp | shell publishes app context | HYPOTHESIS + string xref | no (bypass) | no | shell not loaded |",
                "| gamelist post-update / launch | gamelist.mrp | cfg36 → runapp | HYPOTHESIS | no | no | shell not loaded |",
                "| gbrwshell bridge | gbrwshell.mrp | shell bridge | HYPOTHESIS | no | no | shell not loaded |",
                "| vdload download path | vdload.mrp | download-then-run | HYPOTHESIS | no | no | not on direct jjfb path |",
                "| cfunction/robotol registration | jjfb.mrp members | plugin mrc_extLoad | CROSS_TARGET legacy comment | partial (robotol runs) | no | plugin publication skipped |",
                "| bridge_dsm_mr_start_dsm | host bridge | lowest-layer start | DOCUMENTED | yes | no | not full startGame |",
                "",
                "## Candidate next fix (NOT implemented)",
                "",
                f"- `{nxt}`",
                "- restore missing GWY startGame/runapp context",
                "- call missing legitimate platform publication routine",
                "- fix file/root/cfg param so publication occurs naturally",
                "",
                "Forbidden: fake extChunk, write P+0xC, R9 promotion, skip fault.",
                "",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    # mrc_init gap
    init_seen = m1(text, r"\[JJFB_MRC_INIT_GAP\][^\n]*mrc_init_seen=(\S+)", "no")
    init_note = m1(text, r"\[JJFB_MRC_INIT_GAP\][^\n]*note=(\S+)", "?")
    (reports / "phase6f_mrc_init_gap.md").write_text(
        "\n".join(
            [
                "# Phase 6F — mrc_init gap",
                "",
                f"- mrc_init_seen: `{init_seen}`",
                f"- note: `{init_note}`",
                "",
                "## Answers (observe-only)",
                "",
                "1. Normal plugin/shell paths may call guest `mrc_init` after extChunk is published;",
                "   DOCUMENTED layout uses `mrc_extChunk->sendAppEvent` (@+0x28).",
                "2. Whether `mrc_init` is owned by jjfb vs gwy/startGame is **HYPOTHESIS** without shell execution.",
                "3. Current direct jjfb launch: not observed before NEW_ABI_FAULT (Phase 6E/6F).",
                "4. Fault at `LDR [r0,#0x28]` with NULL P+0xC occurs in post-continuation path —",
                "   consistent with init never reached (**TARGET_OBSERVED** ordering).",
                "5. Missing runapp/shell context remains the leading explanation for skipped publication + init.",
                "",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    # fileopen mapping
    opens = re.findall(
        r'\[JJFB_FILEOPEN\] requested="([^"]*)"[^\n]*host="([^"]*)"[^\n]*ok=(\d+)',
        text,
    )
    misses = re.findall(r'\[JJFB_FILEOPEN_MISS\] guest="([^"]*)"', text)
    fo_lines = [
        "# Phase 6F — file open mapping (240x320 root)",
        "",
        f"opens logged: {len(opens)}  misses logged: {len(misses)}",
        "",
        "| guest | host | ok |",
        "|---|---|---|",
    ]
    for g, h, ok in opens[:80]:
        fo_lines.append(f"| `{g}` | `{h}` | {ok} |")
    fo_lines += ["", "## Misses", ""]
    for g in misses[:40]:
        fo_lines.append(f"- `{g}`")
    if not misses:
        fo_lines.append("- _(none in truncated window)_")
    shell_hits = [g for g, _, _ in opens if "gbrwcore" in g or "gamelist" in g or "gbrwshell" in g]
    fo_lines += [
        "",
        f"shell package opens in live: `{len(shell_hits)}` ({', '.join(shell_hits) or 'none'})",
        "",
    ]
    (reports / "phase6f_fileopen_mapping.md").write_text("\n".join(fo_lines) + "\n", encoding="utf-8")

    print(f"class={klass} bypass={bypass} writes={writes} next={nxt}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
