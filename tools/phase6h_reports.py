#!/usr/bin/env python3
"""Phase 6H: build markdown reports from live stdout + resource root."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def m1(text: str, pat: str, default: str = "no") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def yesno(text: str, needle: str) -> str:
    return "yes" if needle in text else "no"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("report_dir")
    ap.add_argument("gwy_root")
    ap.add_argument("summary_txt")
    args = ap.parse_args()
    text = Path(args.stdout).read_text(encoding="utf-8", errors="replace")
    reports = Path(args.report_dir)
    reports.mkdir(parents=True, exist_ok=True)
    gwy = Path(args.gwy_root)

    gate = m1(text, r"shell_native_exec_gate=(\S+)", "blocked")
    native_class = m1(
        text,
        r"\[JJFB_SHELL_NATIVE_SUMMARY\][^\n]*class=(\S+)",
        m1(text, r"\[JJFB_SHELL_NATIVE_GATE\][^\n]*class=(\S+)", "NONE"),
    )
    shell_class = m1(text, r"\[JJFB_GWY_SHELL_SUMMARY\][^\n]*class=(\S+)", "UNKNOWN")
    guest_pc = yesno(text, "[JJFB_SHELL_GUEST_PC]")
    export_reg = yesno(text, "[JJFB_SHELL_EXPORT]")
    export_call = "yes" if re.search(r"\[JJFB_SHELL_EXPORT_CALL\]", text) else "no"
    shell_ext = yesno(text, "[JJFB_SHELL_EXT]")
    shell_exec = yesno(text, "[JJFB_SHELL_EXEC]")
    host_eq = yesno(text, "host_runapp_equivalent_after_no_update")
    guest_via = "yes" if re.search(r"via=guest_native", text) else "no"
    str601 = "yes" if re.search(r"\[JJFB_STRCOM\][^\n]*code=601|strcom_601=yes", text) else "no"
    str800 = "yes" if re.search(r"\[JJFB_STRCOM\][^\n]*code=800|strcom_800=yes", text) else "no"
    str801 = "yes" if re.search(r"\[JJFB_STRCOM\][^\n]*code=801|strcom_801=yes", text) else "no"
    mrc = "yes" if re.search(r"\[JJFB_MRC_INIT\]|mrc_init=yes", text) else "no"
    pxc = m1(text, r"pxc_writes[^\d]*(\d+)", m1(text, r"pxc_writes_seen=(\d+)", "0"))
    gbrwshell = "yes" if re.search(r"gbrwshell", text) else "no"

    (reports / "phase6h_shell_native_exec.md").write_text(
        "\n".join(
            [
                "# Phase 6H — shell native exec gate",
                "",
                f"- shell_native_exec_gate: `{gate}`",
                f"- native_class: `{native_class}`",
                f"- shell_package_open / SHELL_EXEC: `{shell_exec}`",
                f"- shell_ext_loaded: `{shell_ext}`",
                f"- shell_export_registered: `{export_reg}`",
                f"- shell_export_called: `{export_call}`",
                f"- shell_guest_pc_hit: `{guest_pc}`",
                f"- host_runapp_equivalent present: `{host_eq}` (must be no for mid success)",
                f"- guest_native via tag: `{guest_via}`",
                "",
                "## Ladder",
                "",
                f"- min (gate open / PC+EXT+export): `{'PASS' if gate == 'open' else 'PENDING'}`",
                f"- mid (guest-native runapp/startGame): `{'PASS' if guest_via == 'yes' or export_call == 'yes' else 'PENDING'}`",
                f"- high (_strCom + P+0xC + mrc_init): `{'PASS' if str601 == 'yes' and pxc != '0' and mrc == 'yes' else 'PENDING'}`",
                "",
            ]
        ),
        encoding="utf-8",
    )

    shell_path = gwy / "gbrwshell.mrp"
    (reports / "phase6h_gbrwshell_role.md").write_text(
        "\n".join(
            [
                "# Phase 6H — gbrwshell role",
                "",
                f"- host file present: `{'yes' if shell_path.is_file() else 'no'}`",
                f"- size: `{shell_path.stat().st_size if shell_path.is_file() else 0}`",
                f"- live open/exec evidence: `{gbrwshell}`",
                "",
                "## Role hypotheses (evidence-tagged)",
                "",
                "- Provides shell UI / download list bridge (**TARGET_OBSERVED** strings in prior 6F xref).",
                "- Not proven as the sole P+0xC publisher (**HYPOTHESIS**).",
                "- Must be warmed/executed when present; absence of live PC still means context gap.",
                "",
            ]
        ),
        encoding="utf-8",
    )

    pxc_lines = [
        "# Phase 6H — P+0xC provider",
        "",
        f"- writes_seen: `{pxc}`",
        f"- natural write observed: `{'yes' if pxc not in ('0', 'no') else 'no'}`",
        "",
    ]
    m = re.search(
        r"\[JJFB_P_WRITE\][^\n]*off=0x0C[^\n]*old=(0x[0-9A-Fa-f]+)[^\n]*new=(0x[0-9A-Fa-f]+)"
        r"[^\n]*pc=(0x[0-9A-Fa-f]+)[^\n]*lr=(0x[0-9A-Fa-f]+)[^\n]*module=(\S+)",
        text,
    )
    if m:
        pxc_lines.extend(
            [
                f"- provider module: `{m.group(5)}`",
                f"- provider pc: `{m.group(3)}`",
                f"- caller lr: `{m.group(4)}`",
                f"- old→new: `{m.group(1)}` → `{m.group(2)}`",
            ]
        )
    else:
        pxc_lines.append("- provider: **not observed** (do not fake)")
    pxc_lines.append("")
    (reports / "phase6h_p_extchunk_provider.md").write_text("\n".join(pxc_lines), encoding="utf-8")

    (reports / "phase6h_launch_chain_result.md").write_text(
        "\n".join(
            [
                "# Phase 6H — launch chain result",
                "",
                f"- gwy_shell_class: `{shell_class}`",
                f"- native_class: `{native_class}`",
                f"- shell_native_exec_gate: `{gate}`",
                f"- _strCom 601/800/801: `{str601}/{str800}/{str801}`",
                f"- mrc_init: `{mrc}`",
                f"- P+0xC writes: `{pxc}`",
                f"- host_runapp_equivalent: `{host_eq}`",
                f"- guest_native runapp evidence: `{guest_via}`",
                "",
                "## Success ladder",
                "",
                f"- min: `{'PASS' if gate == 'open' or (shell_exec == 'yes' and guest_pc == 'yes') else 'PENDING'}`",
                f"- mid: `{'PASS' if guest_via == 'yes' or export_call == 'yes' else 'PENDING'}`",
                f"- mid+ (_strCom): `{'PASS' if str601 == 'yes' and str800 == 'yes' and str801 == 'yes' else 'PENDING'}`",
                f"- high (P+0xC + mrc_init): `{'PASS' if pxc not in ('0','no') and mrc == 'yes' else 'PENDING'}`",
                "",
            ]
        ),
        encoding="utf-8",
    )

    summary = Path(args.summary_txt).read_text(encoding="utf-8", errors="replace") if Path(args.summary_txt).is_file() else ""
    print("phase6h reports written")
    print(f"gate={gate} native_class={native_class} host_eq={host_eq}")
    if summary:
        print("summary_ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
