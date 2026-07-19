#!/usr/bin/env python3
"""Build D5 status/roadmap zip for external GPT review."""
from __future__ import annotations

import re
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAMP = "20260719_135359"
NAME = f"JJFB_D5_status_roadmap_pack_{STAMP}"
OUT = ROOT / "packages" / NAME
ZIP_PATH = ROOT / "packages" / f"{NAME}.zip"

BRIEFING = r'''# JJFB / Native GWY Full Boot — Status Pack for Roadmap Advice

**Pack date:** 2026-07-19  
**Overall verdict:** `PARTIAL` — shell chain reaches gamelist init + natural draw + timer fire loop; **no** native_shell runapp / jjfb open / mrc_init.  
**Ask of reviewer (GPT):** propose the next route (D5b+) that respects constraints; prefer smallest discriminating experiments over broad rewrites.

---

## 1. Goal (success definition)

Independent GWY/MRP launcher path for locally owned resources:

```text
gbrwcore → gamelist → cfg36 / no_update → native_shell runapp → jjfb.mrp
→ mrc_loader → robotol → mrc_init → natural draw/refresh
```

**Success logs (must appear):**

```text
[JJFB_RUNAPP] source=native_shell target=gwy/jjfb.mrp
[JJFB_MRC_INIT] ...
```

**Forbidden “success”:**

- patching `jjfb.mrp` / `robotol.ext`
- `host_runapp` / host-side start of jjfb as if shell succeeded
- forcing `ui_mode` / ERW game-state / fake login / fake network complete
- injecting lifecycle events only because the screen did not advance

Target hash (must remain unchanged):

```text
gwy/jjfb.mrp sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036
```

Launch param (already passed into `start_dsm` for gbrwcore and gamelist):

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

---

## 2. What already works (proven)

| Stage | Result | Evidence |
|-------|--------|----------|
| A package scope | `c_load` resolves `gamelist.ext` under package scope | PASS |
| B shell context | gamelist EXTCHUNK + ER_RW + R9 | PASS |
| C/C2/C3 plat alloc | `0x10180` / `0x1010C` / `0x10183` / `0x10119` past NULL faults | PASS |
| C natural draw | `[JJFB_DRAW]` / `[JJFB_REFRESH]` during gamelist init | PASS |
| D1 timer START | classic + chunk-first `sendAppEvent` → `PLATFORM_TIMER START/STOP` | PASS |
| D2 timer FIRE | Nested continue held mutex; fixed with `POST_CONT_PUMP` + EXT helper `code=2` (`mrc_timerTimeout`) | PASS |
| D3 post-timer plat | `0x10204` / `0x10500` tagged alloc; multi-tick OK | PASS |

**Live loop today:** after gamelist init, host loop fires EXT timer every ~5s:  
`FIRE_EXT code=2` → `0x10204`/`0x10500` → re-arm. **9×** cycles observed earlier; **4×** in latest 45s probe boot.

---

## 3. Current blocker (D4/D5)

### 3.1 Discriminating finding

Gamelist **periodic timer path does not call runapp** and **never opens `gwy/cfg.bin`**.

| Observation | Result |
|-------------|--------|
| Host file `game_files/.../gwy/cfg.bin` | exists (20728 bytes) |
| Guest open `cfg.bin` after init | **0** |
| Guest open `jjfb.mrp` after init | **0** |
| Guest-built `[JJFB_GAMELIST_CFG36_BUILD] param=napptype=...` | **0** (only format_string_mapped at image map) |
| `[JJFB_RUNAPP] source=native_shell` | **0** |
| `gamelist.ext` strings `runapp` / `startGame` / `lib.run` | **0** |
| `gamelist.ext` strings `gwy/cfg.bin`, cfg36 fmt, `gwy/%s.mrp` | present |

### 3.2 D5 PC probes (45s boot)

| Probe | Hits |
|-------|------|
| `[JJFB_GAMELIST_CFG_GATE]` | **0** |
| `[JJFB_GAMELIST_CMD_DISP]` | **0** |
| `FIRE_EXT_DONE` | **4** |

**Falsified:** timer FIRE → cfg-open; init path → cfg-open.  
**Open:** command/char dispatcher (`B/H/S/c/...`) never entered; `mr_entry` / table[144] consumption unproven.

### 3.3 Side notes (not proven gates)

- `mythroad/system/gb16.uc2` → `VM_FILE_MISS`
- `mr_plat(1327)` → `MR_IGNORE` (warn); init continues
- `ENTRY_RECONCILE ... relation=HEADER_ENTRY_WRONG` for gamelist (observed first PC ≠ header entry) — helper path still reaches init_ok

---

## 4. Evidence levels (discipline)

Use when proposing fixes:

| Level | Meaning | May become core default? |
|-------|---------|--------------------------|
| DOCUMENTED | SDK / mythroad source / headers | Yes |
| CROSS_TARGET | Same across multiple original MRPs | Yes (versioned) |
| TARGET_OBSERVED | Only JJFB/gamelist trace | Profile/trace only |
| HYPOTHESIS | Not proven | Needs discriminating experiment first |

Do **not** promote HYPOTHESIS/TARGET_OBSERVED into launcher core defaults.

---

## 5. Hard constraints (non-negotiable)

See `constraints/JJFB_LAUNCHER_REBUILD.mdc`.

Short list:

1. No original target MRP byte modification  
2. No fixed JJFB guest addresses / ERW offsets in executable core  
3. No forced UI / game-state writes  
4. No fake login / update / network protocol success  
5. No `host_runapp` counted as shell success  
6. Fail closed; VFS writes → overlay only  
7. Scheduler non-reentrant; timers monotonic  

Shell module-relative offsets for **observation** (e.g. `GAMELIST_OFF_*` in `ext_gwy_shell_native_exec.c`) are TARGET_OBSERVED instrumentation — not a license to hardcode JJFB game addresses into core.

---

## 6. Intended architecture path (MasterPlan)

```text
A package scope
B gamelist platform context
C gamelist reads cfg.bin → cfg36 / no_update / post_update
D native export/dispatcher → lib.runapp / lib.startGame
E open jjfb.mrp (source=native_shell)
F mrc_loader → robotol → mrc_init
G natural resource/draw (no force UI)
```

We are stuck between **C and D**: context/timer OK, but guest never enters cfg/no_update branch, so export/runapp never appears.

DOCUMENTED entry surface: `mr_start_dsm` copies entry into `mr_entry`; Lua `_mr_entry`; `_mr_c_function_table[144]` → `mr_entry` (mythroad.c).

---

## 7. Open questions for roadmap advice

Please answer with ordered experiments (smallest first), each with:

- hypothesis A/B  
- observation points / success logs  
- what would falsify  
- evidence level required before coding into core  

Priority questions:

1. **D5b:** What DOCUMENTED/CROSS_TARGET input should enter gamelist cmd-dispatch / cfg-gate after init — `mr_entry` parse, network command stream, DSM event, gbrwcore export callback — without host-drawn splash or forced game-state UI?  
2. Is passing cfg36 as `start_dsm` entry sufficient for auto-launch, or must guest still read `cfg.bin` index 36?  
3. Should Full Boot chain stop at gamelist UI (needs select), or is auto `nmrpname=gwy/jjfb.mrp` expected without UI?  
4. How to treat `mr_plat(1327)` and missing `gb16.uc2` — ignore vs implement vs resource fix — without TARGET_OBSERVED-as-default?  
5. Where should `lib.runapp` live (gbrwcore vs gamelist vs gbrwshell) given gamelist has no runapp string?

---

## 8. Stale vs current

`reports/fullboot_10_final_verdict.md` is **stale** (still says gamelist init_ok / platform context FAIL).  
**Current truth** is this briefing + `verdicts/phase_d1`…`phase_d5` + `evidence/fullboot_d2_markers_excerpt.txt`.

---

## 9. Pack layout

```text
00_BRIEFING_FOR_GPT.md          ← this file (start here)
verdicts/phase_d1…d5_*.md       ← stage verdicts
reports/fullboot_*.md           ← older fullboot report slices (some stale)
constraints/JJFB_LAUNCHER_REBUILD.mdc
plan/Native_GWY_JJFB_Full_Boot_MasterPlan.md
plan/05_LAUNCH_CONTRACT.md
plan/17_TWO_TRACK_RESEARCH_METHOD.md
evidence/fullboot_d2_markers_excerpt.txt
```

## 10. Suggested reviewer output format

```text
Current bottleneck:
Recommended next 1–3 experiments:
What NOT to do:
When to declare Stage C/D complete:
Risks of regressing into legacy_lab forcing:
```
'''


def main() -> None:
    if OUT.exists():
        shutil.rmtree(OUT)
    for sub in ("verdicts", "reports", "constraints", "plan", "evidence"):
        (OUT / sub).mkdir(parents=True)

    (OUT / "00_BRIEFING_FOR_GPT.md").write_text(BRIEFING, encoding="utf-8")

    for f in (
        "phase_d1_timer_verdict.md",
        "phase_d2_timer_fire_verdict.md",
        "phase_d3_plat_10204_verdict.md",
        "phase_d4_runapp_trigger_verdict.md",
        "phase_d5_cfg_gate_verdict.md",
    ):
        shutil.copy2(ROOT / f, OUT / "verdicts" / f)

    for f in (ROOT / "reports").glob("fullboot_*.md"):
        shutil.copy2(f, OUT / "reports" / f.name)
    shutil.copy2(ROOT / "CONCLUSION.md", OUT / "reports" / "CONCLUSION_root.md")

    # Current superseding status note
    (OUT / "reports" / "00_READ_CURRENT_STATUS_FIRST.md").write_text(
        "# Stale warning\n\n"
        "`fullboot_10_final_verdict.md` and root `CONCLUSION.md` predate Stages D1–D5.\n"
        "Use `../00_BRIEFING_FOR_GPT.md` and `../verdicts/phase_d5_cfg_gate_verdict.md` as current.\n",
        encoding="utf-8",
    )

    shutil.copy2(
        ROOT / ".cursor" / "rules" / "JJFB_LAUNCHER_REBUILD.mdc",
        OUT / "constraints" / "JJFB_LAUNCHER_REBUILD.mdc",
    )
    shutil.copy2(ROOT / "Native_GWY_JJFB_Full_Boot_MasterPlan.md", OUT / "plan")
    shutil.copy2(ROOT / "docs" / "05_LAUNCH_CONTRACT.md", OUT / "plan")
    shutil.copy2(ROOT / "docs" / "17_TWO_TRACK_RESEARCH_METHOD.md", OUT / "plan")

    log = ROOT / "logs" / "fullboot_d2_host_loop_stdout.txt"
    pat = re.compile(
        r"(PACKAGE_SCOPE|GAMELIST_|SHELL_CORE|PLATFORM_TIMER|FIRE_|"
        r"PLATFORM_ALLOC|0x10204|0x10500|cfg\.bin|JJFB_DRAW|JJFB_REFRESH|"
        r"JJFB_RUNAPP|SHELL_EXPORT|CFG_GATE|CMD_DISP|mr_plat|VM_FILE_MISS|"
        r"After app init|HEADER_ENTRY|STARTGAME_EQUIV|GWY_LAUNCH|"
        r"RESOURCE_REQUEST|UPDATE_STUB|EXTCHUNK_SLOT)"
    )
    lines: list[str] = []
    if log.exists():
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if pat.search(line):
                lines.append(line)
        if len(lines) > 480:
            lines = lines[:80] + ["... [truncated middle] ..."] + lines[-400:]
    (OUT / "evidence" / "fullboot_d2_markers_excerpt.txt").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )

    if ZIP_PATH.exists():
        ZIP_PATH.unlink()
    with zipfile.ZipFile(ZIP_PATH, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(OUT.rglob("*")):
            if path.is_file():
                zf.write(path, arcname=str(path.relative_to(OUT.parent)))

    print(ZIP_PATH)
    print("files", sum(1 for _ in OUT.rglob("*") if _.is_file()))
    print("markers", len(lines))
    print("zip_bytes", ZIP_PATH.stat().st_size)


if __name__ == "__main__":
    main()
