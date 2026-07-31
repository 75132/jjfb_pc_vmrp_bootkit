# P22–P24 Fast Closure (honest evidence from run_id 1785510237698)

## Build identity (G0)
- commit: `78cace5307bd3bb524b20d1f265931603d480b3b` (local; requested 688ad0a not present / network fetch failed)
- tree: dirty
- main.exe size/sha256: 5164153 / `64560291b84960adf90cb0432a26242bf7f0e0d1069b8fac8039df7ee0d3af61`
- main_gwy.exe: same binary staged from GwyResearch build (non-zero)
- build_time_utc: 2026-07-31T15:02:22.2286081Z
- mode: `original_headless` research (`JJFB_P22_MODE=original_headless`)
- run_id: 1785510237698
- stop: `UC_ERR_FETCH_UNMAPPED` / `mem_fault` (~25s after gamelist)

## Answers (strict evidence only)

1. **launch param read: YES**
   - `START_DSM_PARAM_ABI_CONFIRMED` for `gwy/gamelist.mrp`
   - `SHELL_PARAM_POINTER_COPIED` + `SHELL_PARAM_GWYBLINK_READ` (param VA `0x682ACC`)

2. **cfg36 real parse: NO**
   - No guest open of `gwy/cfg.bin` (size 20728)
   - No `CFG_FILE_OPENED` / `CFG_RECORD_36_PARSED` in `JJFB_P22_GATE`
   - False-positive path fixed: cfg open and gwyblink string observe no longer call `e10a3_mark_real_cfg_selected`

3. **cfg36 item object + callback: NO**
   - No `CFG36_ITEM_OBJECT_CREATED` / `CFG36_ITEM_SELECTED`
   - Headless auto-click did not fire (no item object)

4. **natural callsite into 0x13A34: PARTIAL / NON-SELECT**
   - Builder PC hit: `base+0x13A34` = `0x2E7D74` (gamelist base `0x2D4340`)
   - LR return = `0x2DF34D` → file off **`0xB00D`** (NOT `0x089CC/0x10136/0x10186/0x1024E`)
   - `state_base=R9+0x6EE` dump was **all zeros** → not a real cfg36-driven construct

5. **constructed descriptor: NOT BUILT**
   - No exact `napptype=12_nextid=482_..._gwyblink` live build observed

6. **0x13B7C object contract: NOT REACHED**

7. **startGame three args: NOT REACHED**
   - `lib.startGame` string VA registered only (`0x30DBB4`); no live `0x2AAD84` enter

8. **opcode300 contract: NOT REACHED** (no nested MRP opcode path after startGame)

9. **nested JJFB: NO** (no nested `gwy/jjfb.mrp` start_dsm from selection)

10. **first screen improved: NO** (blocked before nested)

## Gate matrix (from `JJFB_P22_GATE` / `P22_FINAL` only)

| Gate | Result | Note |
|------|--------|------|
| G0 build | PASS | non-zero research EXE |
| G1 stack depth=2 | PASS* | depth=2 at gamelist start_dsm; frames were cfunction+gbrwcore (not yet proven gamelist-owned child frame) |
| G2 launch param | PASS | ABI + gwyblink read |
| G3 cfg opened | FAIL | hard block |
| G4 cfg36 parsed | FAIL | |
| G5 item created | FAIL | |
| G6 select callback | FAIL | |
| G7 desc builder | PASS* | entered with empty state via off `0xB00D` |
| G8–G14 | FAIL | |

## Hard block

```text
gamelist active + launch param read
→ never opens external gwy/cfg.bin
→ cannot form cfg36 item / original select callback
→ builder probe at 0x13A34 with zero state is not selection
→ UC_ERR_FETCH_UNMAPPED
```

Next discriminating step (not done this round): force progress to `CFG_OPEN_CALLSITE=base+0x0CE8A` without rewriting P21 — inspect why post-init timer/UI never reaches cfg-open, then headless-click only after real item object exists.

## Fixes landed this round

- Separated `CFG_FILE_OPENED` from `real_cfg_selected`
- Stopped treating gwyblink register strings as selected
- Added `p22_selection_gates` breakpoints at documented offsets
- Sticky runtime-stack reporting while gamelist active
- Runner: `RUN_P22_P24_FAST_CLOSURE.ps1`

## Artifacts

- `reports/P22_SELECTION_GATES.csv`
- `reports/P23_STARTGAME_CONTRACT.json`
- `reports/P24_NESTED_JJFB_MATRIX.csv`
- `logs/p22_p24_fast_closure_stdout.txt`
