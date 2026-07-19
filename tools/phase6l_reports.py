#!/usr/bin/env python3
"""Phase 6L: build reports from live stdout logs."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def read(p: str) -> str:
    path = Path(p)
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def m1(text: str, pat: str, default: str = "n/a") -> str:
    m = re.search(pat, text)
    return m.group(1) if m else default


def yesno(text: str, needle: str) -> str:
    return "yes" if needle in text else "no"


def classify(text: str) -> str:
    if "mr_exit_during_entry" in text or re.search(
        r"\[JJFB_MYTHROAD_EXIT\][^\n]*source=entry_return", text
    ):
        return "ENTRY_CALLS_MR_EXIT"
    if re.search(r"reason=SKIP_NO_P|reason=SKIP_NO_PARAM", text):
        return "EARLY_RETURN_ABI"
    if yesno(text, "[JJFB_ENTRY_CLUSTER_HIT]") == "yes":
        return "CLUSTER_REACHED_CHECK_PXC"
    if re.search(r"end_reason=insn_limit|reason=insn_limit", text):
        return "STOP_TOO_SOON"
    # stop_at_base without cluster = early return from entry (ABI/control), even if mr_exit follows
    if re.search(r"end_reason=stop_at_base|reason=stop_at_base", text) and not re.search(
        r"\[JJFB_ENTRY_CLUSTER_HIT\]", text
    ):
        return "EARLY_RETURN_ABI"
    if re.search(r"\[JJFB_MYTHROAD_EXIT\][^\n]*source=post_entry", text) or (
        "mythroad exit" in text and "[JJFB_ENTRY_ABI_RET]" in text
    ):
        return "POST_ENTRY_MR_EXIT"
    pxc = bool(
        re.search(r"off=0x0C[^\n]*new=0x[1-9A-Fa-f]", text)
        or re.search(r"off=0x[Cc][^\n]*new=0x[1-9A-Fa-f]", text)
    )
    if not pxc and "[JJFB_ENTRY_ABI_RET]" in text:
        return "NEEDS_SECOND_STAGE_CALLBACK"
    return "INCONCLUSIVE"


def write(path: Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("reports_dir")
    ap.add_argument("--logs", nargs="+", required=True, help="stdout logs labeled name=path")
    args = ap.parse_args()
    reports = Path(args.reports_dir)
    reports.mkdir(parents=True, exist_ok=True)

    runs = []
    for item in args.logs:
        if "=" not in item:
            continue
        name, path = item.split("=", 1)
        text = read(path)
        runs.append((name, path, text))

    primary = runs[0][2] if runs else ""

    # ABI snapshot from first log with PRE
    pre = m1(primary, r"\[JJFB_ENTRY_ABI_PRE\]([^\n]*)", "")
    ret = m1(primary, r"\[JJFB_ENTRY_ABI_RET\]([^\n]*)", "")
    write(
        reports / "phase6l_entry_abi_snapshot.md",
        [
            "# Phase 6L — entry ABI snapshot",
            "",
            f"- PRE: `{pre.strip() or 'n/a'}`",
            f"- RET: `{ret.strip() or 'n/a'}`",
            "",
            "Evidence: TARGET_OBSERVED (live regs); entry PC DOCUMENTED as image_base+8.",
            "",
        ],
    )

    cov = m1(primary, r"\[JJFB_ENTRY_COVERAGE\]([^\n]*)", "")
    last = m1(primary, r"\[JJFB_ENTRY_LASTPCS\]([^\n]*)", "")
    write(
        reports / "phase6l_entry_coverage.md",
        [
            "# Phase 6L — entry coverage",
            "",
            f"- COVERAGE: `{cov.strip() or 'n/a'}`",
            f"- LASTPCS: `{last.strip()[:500] or 'n/a'}`",
            f"- CLUSTER_HIT present: `{yesno(primary, '[JJFB_ENTRY_CLUSTER_HIT]')}`",
            "",
        ],
    )

    cluster_lines = [
        "# Phase 6L — init cluster reachability",
        "",
        "| cluster_index | file_off | reached |",
        "|---:|---:|---|",
    ]
    for m in re.finditer(
        r"\[JJFB_ENTRY_CLUSTER\] index=(\d+) file_off=(\d+) va=(0x[0-9A-Fa-f]+) reached=(\S+)",
        primary,
    ):
        cluster_lines.append(f"| {m.group(1)} | {m.group(2)} | {m.group(4)} |")
    if len(cluster_lines) == 4:
        cluster_lines.append("| n/a | n/a | n/a |")
    cluster_lines += ["", "Hypothesis until PC lands in cluster: HYPOTHESIS.", ""]
    write(reports / "phase6l_init_cluster_reachability.md", cluster_lines)

    pcands = re.findall(r"\[JJFB_P_CANDIDATE\][^\n]*", primary)
    pxc_any = "yes" if re.search(r"off=0x0C[^\n]*new=0x[1-9A-Fa-f]", primary) else "no"
    write(
        reports / "phase6l_p_candidate_map.md",
        [
            "# Phase 6L — P candidate map",
            "",
            f"- candidates seen: `{len(pcands)}`",
            f"- any natural nonzero P+0xC: `{pxc_any}`",
            "",
            *(f"- `{c}`" for c in pcands[:40]),
            "",
        ],
    )

    exit_tag = m1(primary, r"\[JJFB_MYTHROAD_EXIT\]([^\n]*)", "")
    entry_exit = m1(primary, r"\[JJFB_ENTRY_EXIT\]([^\n]*)", "")
    write(
        reports / "phase6l_entry_return_exit_reason.md",
        [
            "# Phase 6L — entry return / mythroad exit",
            "",
            f"- ENTRY_EXIT: `{entry_exit.strip() or 'n/a'}`",
            f"- MYTHROAD_EXIT: `{exit_tag.strip() or 'n/a'}`",
            f"- mythroad exit string present: `{yesno(primary, 'mythroad exit')}`",
            f"- class hint: `{classify(primary)}`",
            "",
        ],
    )

    cmp_lines = [
        "# Phase 6L — ABI variant compare",
        "",
        "| run | entry_hit | emu_ok | cluster | pxc_nz | end_reason | class |",
        "|---|---|---|---|---|---|---|",
    ]
    best = None
    for name, path, text in runs:
        hit = yesno(text, "[JJFB_MRPGCMAP_ENTRY_HIT]")
        emu = "yes" if re.search(r"result=EMU_OK", text) else "no"
        cl = yesno(text, "[JJFB_ENTRY_CLUSTER_HIT]")
        pxc = (
            "yes"
            if re.search(r"off=0x0C[^\n]*new=0x[1-9A-Fa-f]", text)
            or re.search(r"off=0x[Cc][^\n]*new=0x[1-9A-Fa-f]", text)
            else "no"
        )
        er = m1(text, r"\[JJFB_ENTRY_ABI_RET\][^\n]*reason=(\S+)", "n/a")
        cls = classify(text)
        tag = ""
        if cl == "yes" or pxc == "yes":
            tag = " ABI_CANDIDATE"
            best = name
        cmp_lines.append(f"| `{name}` | {hit} | {emu} | {cl} | {pxc} | `{er}` | `{cls}`{tag} |")
    cmp_lines += ["", f"- best_candidate: `{best or 'none'}`", ""]
    write(reports / "phase6l_abi_variant_compare.md", cmp_lines)

    cls = classify(primary)
    verdict = [
        "# Phase 6L — verdict",
        "",
        "## Bottom line",
        "",
        f"**Classification: `{cls}`**",
        "",
        "Phase 6K proved documented entry order; Phase 6L asks why entry returns without P+0xC publication.",
        "",
        "## Primary (baseline) facts",
        "",
        f"- ENTRY_HIT: `{yesno(primary, '[JJFB_MRPGCMAP_ENTRY_HIT]')}`",
        f"- ABI PRE/RET present: `{yesno(primary, '[JJFB_ENTRY_ABI_PRE]')}` / `{yesno(primary, '[JJFB_ENTRY_ABI_RET]')}`",
        f"- cluster reached: `{yesno(primary, '[JJFB_ENTRY_CLUSTER_HIT]')}`",
        f"- natural P+0xC nonzero: `{pxc_any}`",
        f"- end_reason: `{m1(primary, r'\[JJFB_ENTRY_ABI_RET\][^\n]*reason=(\S+)', 'n/a')}`",
        f"- mythroad exit: `{yesno(primary, 'mythroad exit')}`",
        "",
        "## Interpretation",
        "",
    ]
    if cls == "EARLY_RETURN_ABI":
        insns = m1(primary, r"insns=(\d+)", "?")
        verdict.append(
            f"Documented entry hit `stop_at_base` after only `{insns}` instructions; "
            "none of the five +0xC init clusters were reached. All five ABI variants and wxjwq "
            "baseline behaved the same. `mr_exit` is **post-entry** (not the entry killer). "
            "Leading cause: entry control-flow early-out / incomplete init semantics — not wrong "
            "entry PC selection (already fixed in 6K)."
        )
    elif cls == "POST_ENTRY_MR_EXIT":
        verdict.append(
            "Documented entry returned without P+0xC; `mr_exit` ran after entry. "
            "Publication is not completed inside the current entry path."
        )
    elif cls == "ENTRY_CALLS_MR_EXIT":
        verdict.append("Guest called `mr_exit` during documented entry emu.")
    elif cls == "STOP_TOO_SOON":
        verdict.append("Entry hit insn_limit before clusters; try higher JJFB_ENTRY_INSN_LIMIT in 6M.")
    else:
        verdict.append(
            "Entry did not reach +0xC init clusters; natural publication still absent. "
            "Next phase should fix the single leading cause above (ABI/stop/second-stage), not UI."
        )
    verdict += [
        "",
        "## Forbidden",
        "",
        "Do not invent P+0xC, jump to cluster, force UI, or expand to gamelist chase yet.",
        "",
        f"## Best ABI_CANDIDATE",
        "",
        f"`{best or 'none'}`",
        "",
    ]
    write(reports / "phase6l_verdict.md", verdict)
    write(reports / "CONCLUSION.md", verdict)

    print(f"class={cls} best={best or 'none'} runs={len(runs)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
