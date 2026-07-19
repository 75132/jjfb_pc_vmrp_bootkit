#!/usr/bin/env python3
"""Phase 6M: build publication-source reports from live stdout."""
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


def write(path: Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("reports_dir")
    ap.add_argument("--wx-stdout", default="")
    args = ap.parse_args()
    reports = Path(args.reports_dir)
    reports.mkdir(parents=True, exist_ok=True)
    text = read(args.stdout)
    wx = read(args.wx_stdout) if args.wx_stdout else ""

    # --- timeline ---
    tl = re.findall(r"\[JJFB_P_TIMELINE\]([^\n]*)", text)
    rows = [
        "# Phase 6M — P struct timeline",
        "",
        "| seq | excerpt |",
        "|---:|---|",
    ]
    for i, line in enumerate(tl[:80], 1):
        rows.append(f"| {i} | `{line.strip()[:200]}` |")
    if len(tl) == 0:
        rows.append("| n/a | no JJFB_P_TIMELINE tags |")
    rows += [
        "",
        f"- timeline events: `{len(tl)}`",
        f"- 0x94F04 zero writer: `{yesno(text, 'pc=0x94F04')}`",
        f"- CFN_PXC_SOURCE: `{yesno(text, '[JJFB_CFN_PXC_SOURCE]')}`",
        "",
    ]
    write(reports / "phase6m_p_struct_timeline.md", rows)

    # --- chunk_field_04 ---
    write(
        reports / "phase6m_chunk_field_04_source.md",
        [
            "# Phase 6M — chunk_field_04 source",
            "",
            "## DOCUMENTED layout (do not conflate)",
            "",
            "- `mr_c_function_st + 0x0C` = `mrc_extChunk*` (watched `P+0xC`)",
            "- `mrc_extChunk_st + 0x04` = `init_func` (registry `chunk_field_04`)",
            "",
            "## Live",
            "",
            f"- CHUNK_FIELD04 tags: `{len(re.findall(r'\[JJFB_CHUNK_FIELD04\]', text))}`",
            f"- MISSING tags: `{yesno(text, '[JJFB_CHUNK_FIELD04_MISSING]')}`",
            f"- EXT_REGISTER chunk_field_04=0: `{yesno(text, 'chunk_field_04=0x0')}`",
            f"- writer NONE_BEFORE_SELECT: `{yesno(text, 'NONE_BEFORE_SELECT')}`",
            "",
            "### Interpretation",
            "",
            "`chunk_field_04=0` means no observed write of **chunk+4 (init_func)** before DSM select, "
            "which is consistent with **no published `mrc_extChunk` object** (P+0xC stays 0 / zeroed). "
            "It is not the same field as P+0xC.",
            "",
        ],
    )

    # --- contract ---
    write(
        reports / "phase6m_mr_c_function_new_contract.md",
        [
            "# Phase 6M — `_mr_c_function_new` contract",
            "",
            "## DOCUMENTED (mythroad.c / fixR9.c)",
            "",
            "1. Host allocates `mr_c_function_P` of `len` bytes",
            "2. Host `memset` zeros the P struct",
            "3. Host stores helper (`MR_C_FUNCTION`)",
            "4. Returns `MR_SUCCESS`",
            "5. **Guest/ext** fills `start_of_ER_RW` / `ER_RW_Length` after return",
            "6. Publishing `mrc_extChunk*` into `P+0xC` is a **separate** step (not done by host new)",
            "",
            "## LIVE host (bridge.c `br__mr_c_function_new`)",
            "",
            f"- CFN_NEW_CONTRACT seen: `{yesno(text, '[JJFB_CFN_NEW_CONTRACT]')}`",
            f"- CFUNCTION_NEW_SIDE_EFFECT NO_DSM_DISPATCH: `{yesno(text, 'NO_DSM_DISPATCH_WRITE')}`",
            f"- host_memset note: `{yesno(text, 'host_memset=yes')}`",
            "",
            "### Verdict",
            "",
            "Host zero-init of the 20-byte P is **expected** (DOCUMENTED). "
            "Guest `dsm:cfunction.ext @ 0x94F04` also zero-stores the P fields (TARGET_OBSERVED). "
            "Missing piece is natural **`mrc_extChunk` create/publish** into P+0xC / chunk+4 — "
            "not more entry ABI variants.",
            "",
            "Next phase direction (do not invent in 6M): restore legitimate extChunk "
            "allocation/register path if DOCUMENTED/CROSS_TARGET contract requires it.",
            "",
        ],
    )

    # --- candidates ---
    cands = re.findall(r"\[JJFB_EXTCHUNK_CANDIDATE\][^\n]*", text)
    write(
        reports / "phase6m_extchunk_candidate_structs.md",
        [
            "# Phase 6M — natural extChunk candidates",
            "",
            f"- candidates with check magic: `{len(cands)}`",
            "",
            *(f"- `{c}`" for c in cands[:40]),
            "",
            "If none: no natural `0x7FD854EB` header peeked near P/helper during 6M — "
            "do not invent.",
            "",
        ],
    )

    saw94 = yesno(text, "saw_94f04_zero=yes") == "yes" or yesno(text, "pc=0x94F04") == "yes"
    pxc_nz = yesno(text, "pxc_nz=yes") == "yes" or bool(
        re.search(r"off=0x0C[^\n]*new=0x[1-9A-Fa-f]", text)
    )
    klass = "CFUNCTION_PUBLICATION_SOURCE_ZERO"
    if not saw94 and yesno(text, "CHUNK_FIELD04_MISSING") == "yes":
        klass = "CHUNK_FIELD_04_SOURCE_MISSING"

    wx_same = "n/a"
    if wx:
        wx_same = (
            "yes"
            if ("0x94F04" in wx and "CHUNK_FIELD04_MISSING" in wx)
            or ("pc=0x94F04" in wx)
            else "partial"
        )

    verdict = [
        "# Phase 6M — verdict",
        "",
        "## Bottom line",
        "",
        f"**Classification: `{klass}`**",
        "",
        "P+0xC is not 'unwritten'; `dsm:cfunction.ext @ 0x94F04` **writes 0** across the "
        "20-byte P (including +0xC). Host `_mr_c_function_new` also memset-zeros P (DOCUMENTED). "
        "`chunk_field_04=0` / `NONE_BEFORE_SELECT` means no published extChunk init_func "
        "(chunk+4), distinct from P+0xC.",
        "",
        "## Facts",
        "",
        f"- CFN enter/disasm tags: `{yesno(text, '[JJFB_CFN_DISASM]')}`",
        f"- 0x94F04 zero path: `{saw94}`",
        f"- natural P+0xC nonzero: `{pxc_nz}`",
        f"- chunk_field_04 missing: `{yesno(text, '[JJFB_CHUNK_FIELD04_MISSING]')}`",
        f"- extChunk magic candidates: `{len(cands)}`",
        f"- wxjwq same zero-writer (if run): `{wx_same}`",
        "",
        "## Forbidden",
        "",
        "Do not invent P+0xC, hardcode chunk, jump clusters, retry entry ABI matrix, or chase UI.",
        "",
        "## Next phase (6N) unique direction",
        "",
        "Restore/locate the **legitimate** `mrc_extChunk` allocation/register publication "
        "so P+0xC and chunk+4 become natural nonzero — only with DOCUMENTED/CROSS_TARGET "
        "contract evidence.",
        "",
    ]
    write(reports / "phase6m_verdict.md", verdict)
    write(reports / "CONCLUSION.md", verdict)
    print(f"class={klass} saw94={saw94} pxc_nz={pxc_nz} cands={len(cands)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
