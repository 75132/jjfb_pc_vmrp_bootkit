# Stage E8O-Fast Verdict

**Verdict:** `FAST_REACHED_30103C_NEXT_GAP`

**NOT product success.** FAST_ASSIST only — no MRP/EXT edits, no fake DRAW/UI.

Default SVC mode: `return0` (plus one `observe` capture). SVC `#0xAB` was **never hit** on any matrix cell — the post-gate SVC trap did not engage because the path never reached that depth.

---

## Headline

With controlled `R9+0x8D0` poke + host `0x10102` case156:

| Assist | Result |
| --- | --- |
| `state=20` | reaches `0x300714`, **stops before** `0x30103C` |
| `state=38` | reaches `0x30103C` → **`0x3020C8`** |
| after `0x3020C8` | state cleared back to `0`; **no C44/C9D/CF5**; **no SVC**; **no DRAW** |
| `0x302340` BP | **NEVER_ENTERED** (R1=18 arm exits earlier) |
| R1=20 arm | hits `writer_pc=0x302360` (near C44 path) but C44 stays 0 |

**Next real blocker (FAST-exposed):** inside / after `0x3020C8` case arms — reach C44 writer `0x2F4E82` needs live `R9+0xC6C` / `0xEEC` / `0x11D0` (still 0). Not SVC. Not DRAW yet.

---

## Matrix (PARENT_HIT only)

| Run | 158 | 714 | 30103C | 3020C8 | state clear PC | C44* | SVC | DRAW | after_sibling notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s20_r18 | Y | Y | N | N | — | N | N | N | state=0x14 E6C=0 7D8=0x10000 |
| s20_r20 | Y | Y | N | N | — | N | N | N | state=0x14 E6C=0 7D8=0x10000 |
| s38_r18 | Y | Y | Y | Y | **0x3021FA** | N | N | N | state→0 E6C=0 |
| s38_r20 | Y | Y | Y | Y | **0x302360** | N | N | N | state→0 E6C=0 |
| 10165_s38_r18 | Y | Y | Y | Y | **0x3021FA** | N | N | N | B7D=1 FE8≠0; still no C44 |
| 10165_s38_r20 | Y | Y | Y | Y | **0x302360** | N | N | N | B7D=1 FE8≠0; still no C44 |
| 310_s38_r18 | Y | Y | Y | Y | **0x3021FA** | N | N | N | E6C still 0 (310 null-arm) |
| 10165_310_s38_r20 | Y | Y | Y | Y | **0x302360** | N | N | N | B7D=1; E6C=0; no C44 |
| s38_r18_svc_observe | Y | Y | Y | Y | **0x3021FA** | N | N | N | no SVC dump (never reached) |

\*C44/C9D/CF5 `FLAG_TRANSITION` — none observed.

---

## Evidence (canonical s38_r18)

```text
[JJFB_FAST_ASSIST] enabled=1 state=0x26 case156_r1=0x12 seq=case156 svc_ab=r
[JJFB_FAST_STATE] old=0x0 new=0x26
[JJFB_E8I_PARENT_HIT] tag=p300714 ... state=0x26
[JJFB_E8I_PARENT_HIT] tag=p30103C ... state=0x26
[JJFB_E8I_PARENT_HIT] tag=p3020C8 ... state=0x26
[JJFB_E8I_STATE_WRITE] old=0x26 new=0x0 writer_pc=0x3021FA
[JJFB_E8F_FLAG_SNAP] after_sibling C44=0 C9D=0 CF5=0 E6C=0 R9_state=0x0 R9_7D8=0x10000
[JJFB_FAST_FIRE_DONE] svc_hits=0 svc_continues=0
[JJFB_E8F_WRITER_NEVER] tag=p302340 class=NEVER_ENTERED
```

R1=20 variant (s38_r20) clears via **`0x302360`** (adjacent to historically documented C44 arm `0x302362`) and returns `end=stop_at_base` — still no C44 write.

---

## What FAST ruled out / ruled in

| Hypothesis | FAST result |
| --- | --- |
| state=38 + case156 enough for DRAW? | **No** |
| SVC `#0xAB` is the immediate next gap after 30103C? | **No** — never reached |
| C44/C9D/CF5 flip on this assist alone? | **No** |
| 10165 (FE8/B7D) unlocks C44 on this path? | **No** — B7D/FE8 set, C44 still 0 |
| case310 unlocks main path? | **No** — E6C remains 0; same 3020C8 exit |
| Next gap location | **`0x3020C8` case arms → C44 writer deps** (`R9+C6C/EEC/11D0`), then maybe SVC |

Cross-ref E8H: `0x3020C8` switches on parent arg (here case156 R1); arms toward `0x302340`/`0x302362` → `BL 0x2F4E64` → C44 writer `0x2F4E82`, which uses `R9+0xC6C` / `0xEEC` / `0x11D0`. Live snaps show those still 0 at dispatcher entry.

---

## Clean backfill (B 线) — priority order

1. **Naturalize `R9+0x8D0` writer** (still required for product; FAST only poked it). Hot candidates remain `0x2DC80C` / `0x2DD068`.
2. **Trace why `0x3020C8` clears state and exits without C44** — especially gates before `0x2F4E82` on `C6C`/`EEC`/`11D0` object init.
3. **Do not prioritize SVC `#0xAB` handler yet** — FAST never reached it on this ladder.
4. E6C init remains relevant for case310’s non-null arm, but was **not** the blocker that stopped the state=38+case156 main path at C44.

---

## Artifacts

- `RUN_E8O_FAST.ps1`
- `logs/stage_e8o_*_stdout.txt`
- Env: `JJFB_FAST_ASSIST`, `JJFB_FAST_STATE`, `JJFB_FAST_CASE156_R1`, `JJFB_FAST_SEQUENCE`, `JJFB_FAST_SVC_AB`
- Runtime: `src/runtime/robotol_flag_writer_trace.c` (FAST tags + guarded SVC continue)

---

## Note on auto-report bug

The first auto-generated matrix table falsely showed all `False` (pattern matched `WRITER_NEVER` / missed `PARENT_HIT`). This document is the corrected recount from the same log files.
