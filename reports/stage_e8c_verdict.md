# Stage E8C — Robotol Idle State-Machine Exit Condition

## Verdict

**ROBOTOL_STATE_FLAG_NEVER_SET**

After join `0x3066AD`, robotol polls five R9-relative flags. Across 40 lifecycle ticks (handler `ok=1`), all stay `0`, no `UC_HOOK_MEM_WRITE` hits the watched cells, and plat `0x1E209/0x9` produces `watched_writes=0`. No natural DRAW. Idle exit conditions never become true.

## Flag map (static)

| Site | ER_RW off | Width | CMP | Fail branch |
|------|-----------|-------|-----|-------------|
| 0x3066AC | 0xC44 | s8 | ==1 | 0x3066BD |
| 0x3066BC | 0xC9D | s8 | ==1 | 0x306745 |
| 0x3066C8 | 0xCD1 | s8 | ==0 | 0x306745 |
| 0x3066D2 | 0xCF5 | s8 | ==1 | 0x306741 |
| 0x3066DC | 0x11B0 | u32 | ==0 | 0x306741 |

Guest address = lifecycle R9 (`0x2B1858`) + offset. Watch armed with `offset_base=0x2B1858` (not raw `start_of_er_rw=0x2B1854`).

## Live observe

- Init all `0`; tick 40 still all `0`; `FLAG_WRITE=0`, `FLAG_TRANSITION=0`.
- `[JJFB_E8C_HELPER_FX] r0=0x1E209 r1=0x9 ret=0 watched_writes=0` every tick — **ret not mutated**.
- Static xref: `0xC9D` has **no** STR/STRB sites in `robotol.ext` (strong never-set candidate). Other offsets have writers classified mostly `robotol_internal` / `unknown` — not reached in this run.

## Gates

| Gate | Pass |
|------|------|
| No Thumb/ABI / 0x1E209 ret edits | yes |
| Flag resolve + map | `out/e8c_tmp/flag_map.json` |
| Write xref | `out/e8c_tmp/flag_write_xref.md` |
| Idle watch armed | yes |
| Long run | 40 ticks unchanged (wall ~75s incl. boot) |
| audit_launcher_core | ok |
| jjfb.mrp hash | unchanged |

## Artifacts

- Runner: `RUN_E8C_IDLE_STATE_EXIT.ps1`
- Log: `logs/stage_e8c_jjfb_stdout.txt`
- Module: `src/runtime/robotol_idle_watch.c`

## Next stage (not E8C)

Trace **why writers to `0xC44` / `0xCD1` / `0xCF5` / `0x11B0` never run** (and whether `0xC9D` is set only via an undocumented path). Likely waiting for an app/event/input/network path that feeds those robotol-internal writers — still observe-only; do not force flags.
