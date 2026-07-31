# P25 Gamelist Config State Machine + Control Flow

## Build identity
- commit: `31f700d06e799c7dab25a8bbe39588d9091c4ddf`
- tree: dirty
- main.exe: 5218491 / `9b7d960316c3c06b64b54f4fd4ca51f087649e1fbda94003d251a7f16515f6a8`
- main_gwy.exe: 5218491 / `9b7d960316c3c06b64b54f4fd4ca51f087649e1fbda94003d251a7f16515f6a8`
- build_time_utc: 2026-07-31T18:43:28.8272217Z
- run_id: 1785523410052
- seconds: 55

## Answers (required)

1. **0x10112 ABI**: IMPLEMENTED, runtime NOT_SEEN — `sendAppEvent(0x10112, ctx=R9+0x46C, path, *out_buf, *out_len, …)` → `GWY_PLAT_KIND_READ_FILE`. Bare `cfg.bin` → `MRP_MEMBER`; `gwy/…` → `SHARED_ROOT`. No member→shared alias.
2. **Internal cfg.bin loaded**: NO — never hit `base+0x7B6C`.
3. **External cfg state machine**: NOT ENTERED — `base+0xD768` not hit.
4. **gwy/cfg.bin read**: NO.
5. **Stray builder source**: NO — prior LR≈B00D hit was **false positive** (aligned vs raw base pad=0x14 / getter `0x13A20`). Legal LR set `{0x089D1,0x1013B,0x1018B,0x10253}`.
6. **Fetch unmapped source**: NO (closed this run — EXIT_PARK + stop_hook / no _mr_ putc). Proven producer when it fired: DSM `0x8E6D0` `blx ip` with `ip` = LE word of `_mr_c_function_new` after EXIT_PARK resumed sticky/DSM (`function_entry=0x805F8`). Not descriptor-builder pollution.
7. **cfg36 item**: parsed=NO item=NO.
8. **startGame/opcode300/nested**: all NO.

## Gate matrix
All G1–G17 FAIL (blocked before G1).

## What this round corrected
- Renamed false cfg BPs: `+0x01AF8` → STATE_SLOT_COPY_438_TO_430; `+0x0CE8A` → CONDITIONAL_STATE_SLOT_COMMIT (diag only).
- Installed real cfg sites: `+0x07B6C` loader, `+0x07B9C` dispatch, callers, `+0x0D768` path state.
- `0x10112` platform READ_FILE + VFS namespace split.
- EXIT_PARK: do not treat live `CODE_ADDRESS` as a stop; RO page `0xE7F000`; POST_CONT ends on first fire when `wait_for_timer=0`; global stop_hook for park.
- Documented gamelist entry remains opt-in (`JJFB_GAMELIST_DOCUMENTED_ENTRY=1`); default skip preserves POST_CONT.

## Current blocker
`EXIT_PARK` still does not reliably hard-break the outer sticky `runCode` into the host loop. After park the process often idles until runner timeout with no `CFG_LOADER_ENTRY`. Until that CF handoff is clean, internal `cfg.bin` 6898 / external 20728 / cfg36 cannot proceed.

## Next discriminating step
Force outer `runCode` to break on `g_e10a_exit_parked` without depending on PC landing on the RO page (e.g. setjmp out of `runCode`, or stop_hook that fires on the first non-exit stub after park and then `runCode_break`). Then re-check G1 at `base+0x7B6C`.

## Artifacts
- `research/packs/p25_cfg_state/P25_TRACE.csv`
- `reports/P25_GAMELIST_CONFIG_AND_CONTROL_FLOW.md`
- `logs/p25_cfg_state_stdout.txt`