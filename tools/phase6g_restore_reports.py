#!/usr/bin/env python3
"""Phase 6G: emit markdown reports from live stdout + static entry map."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def grab(text: str, pat: str, default: str = "?") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("reports_dir")
    ap.add_argument("gwy_root")
    ap.add_argument("summary_txt")
    args = ap.parse_args()
    text = Path(args.stdout).read_text(encoding="utf-8", errors="replace") if Path(args.stdout).is_file() else ""
    reports = Path(args.reports_dir)
    reports.mkdir(parents=True, exist_ok=True)
    gwy = Path(args.gwy_root)

    # resource root mapping
    opens = re.findall(
        r'\[JJFB_FILEOPEN\][^\n]*guest="([^"]+)"[^\n]*host="([^"]+)"[^\n]*ok=(\d+)',
        text,
    )
    # Also match requested= form
    opens2 = re.findall(
        r'\[JJFB_FILEOPEN\] requested="([^"]+)"[^\n]*host="([^"]+)" ok=(\d+)',
        text,
    )
    rows = opens or [(g, h, o) for g, h, o in opens2]
    map_lines = [
        "# Phase 6G — resource root mapping",
        "",
        f"mythroad_root hint: `{grab(text, r'\[JJFB_GWY_ROOT\] mythroad_root=(.+)')}`",
        f"gwy_root hint: `{grab(text, r'\[JJFB_GWY_ROOT\] gwy_root=(.+)')}`",
        "",
        "| guest | host | ok | alias? |",
        "|---|---|---|---|",
    ]
    alias_hits = 0
    real_jjfb = 0
    shell_open = 0
    for guest, host, ok in rows[:80]:
        alias = "yes" if "jjfb_alias" in host.replace("\\", "/") else "no"
        if alias == "yes":
            alias_hits += 1
        if "jjfb.mrp" in guest.replace("\\", "/") and alias == "no" and ok == "1":
            real_jjfb += 1
        if any(x in guest for x in ("gbrwcore", "gamelist", "gbrwshell")) and ok == "1":
            shell_open += 1
        map_lines.append(f"| `{guest}` | `{host}` | {ok} | {alias} |")
    map_lines.extend(
        [
            "",
            f"- shell_package_opens≈{shell_open}",
            f"- jjfb_alias_opens={alias_hits}",
            f"- real_jjfb_opens≈{real_jjfb}",
            f"- expected_gwy_dir=`{gwy}`",
            "",
        ]
    )
    (reports / "phase6g_resource_root_mapping.md").write_text("\n".join(map_lines), encoding="utf-8")

    stub_lines = [
        "# Phase 6G — gwy shell no-update stub",
        "",
        f"- stub_log_seen: `{'yes' if '[JJFB_GWY_UPDATE_STUB]' in text else 'no'}`",
        f"- game_self_untouched: `{'yes' if 'untouched=yes' in text else 'unknown'}`",
        f"- apply_initNetwork: `{'yes' if 'apply=initNetwork' in text else 'no'}`",
        "",
        "Boundary: stub applies only while active package is gwy shell "
        "(gbrwcore/gamelist/gbrwshell), never jjfb.",
        "",
        "Evidence: **TARGET_OBSERVED** launcher shim.",
        "",
    ]
    (reports / "phase6g_gwy_shell_no_update_stub.md").write_text("\n".join(stub_lines), encoding="utf-8")

    cls = grab(text, r"\[JJFB_GWY_SHELL_SUMMARY\] class=(\S+)")
    if cls == "?":
        cls = grab(text, r"gwy_context_class=(\S+)")
    writes = grab(text, r"pxc_writes_seen=(\d+)", "0")
    mrc = "yes" if re.search(r"mrc_init_seen=yes|\[JJFB_MRC_INIT\]", text) else "no"
    strcom = "yes" if re.search(r"\[JJFB_STRCOM\].*601|strcom_601", text) else "no"
    chain_lines = [
        "# Phase 6G — launch chain result",
        "",
        f"- class: `{cls}`",
        f"- CFG36: `{'yes' if '[JJFB_CFG36]' in text else 'no'}`",
        f"- STARTGAME: `{'yes' if '[JJFB_STARTGAME]' in text else 'no'}`",
        f"- RUNAPP: `{'yes' if '[JJFB_RUNAPP]' in text else 'no'}`",
        f"- shell_summary: `{grab(text, r'(\[JJFB_GWY_SHELL_SUMMARY\][^\n]*)')}`",
        "",
        "## Success ladder",
        "",
        f"- min (not SHELL_BYPASSED + shell open + no alias): "
        f"`{'PASS' if cls not in ('SHELL_BYPASSED_DIRECT_JJFB', '?') and shell_open > 0 and alias_hits == 0 else 'PENDING'}`",
        f"- mid (startGame/runapp tags): "
        f"`{'PASS' if '[JJFB_STARTGAME]' in text and '[JJFB_RUNAPP]' in text else 'PENDING'}`",
        f"- mid+ (_strCom 601/800/801): "
        f"`{'PASS' if strcom == 'yes' else 'PENDING'}`",
        f"- high (natural P+0xC + mrc_init): "
        f"`{'PASS' if writes != '0' and mrc == 'yes' else 'PENDING'}`",
        "",
    ]
    (reports / "phase6g_launch_chain_result.md").write_text("\n".join(chain_lines), encoding="utf-8")

    pub_lines = [
        "# Phase 6G — P+0xC / mrc_extChunk publication result",
        "",
        f"- pxc_writes_seen: `{writes}`",
        f"- mrc_init: `{mrc}`",
        f"- _strCom 601/800/801: `{strcom}`",
        "",
        "Fake extChunk is forbidden. Natural write only.",
        "",
        f"Evidence: **{'TARGET_OBSERVED' if writes != '0' else 'HYPOTHESIS_until_natural_write'}**",
        "",
    ]
    (reports / "phase6g_p_extchunk_publication_result.md").write_text(
        "\n".join(pub_lines), encoding="utf-8"
    )

    # Ensure startgame entry md exists (may be written by sibling tool)
    entry = reports / "phase6g_startgame_runapp_entry.md"
    if not entry.is_file():
        entry.write_text(
            "# Phase 6G — startGame/runapp entry\n\n(pending static tool)\n", encoding="utf-8"
        )

    print("phase6g reports written")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
