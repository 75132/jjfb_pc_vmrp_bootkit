# D5b — command dispatch trigger result (45s quiet boot)

**Date:** 2026-07-19  
**Blocker name:** `GAMELIST_COMMAND_SOURCE_NOT_DELIVERED`  
**Verdict class:** `TIMER_ONLY_CONFIRMED_DEAD` + `PARAM_NOT_CONSUMED`

## Runtime counters

| Marker | Count |
|--------|------:|
| `[JJFB_PARAM_MAP]` | 2 |
| `[JJFB_PARAM_READ]` | **0** |
| `[JJFB_GAMELIST_HANDLER_MAP]` | 16 (incl. bad seed; live 0x10102 also present) |
| `[JJFB_GAMELIST_HANDLER_ENTER]` | **0** |
| `[JJFB_GAMELIST_CMD_DISP]` | **0** |
| `[JJFB_GAMELIST_CFG_GATE]` | **0** |
| `cfg.bin` open | **0** |
| `FIRE_EXT_DONE` | 4 |
| `[JJFB_RUNAPP] source=native_shell` | **0** |

## PARAM_MAP samples (DOCUMENTED)

```text
[JJFB_PARAM_MAP] va=0x2829FC len=79 param="napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
[JJFB_PARAM_MAP] va=0x682ACC len=79 param="napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
```

(gbrwcore then gamelist nested `start_dsm` entry copies)

## Live 0x10102 registrations (TARGET_OBSERVED)

Seen via slot28 (seeded=0):

```text
0x10600 → 0x2E74AD
0x10601 → 0x2E03E1
0x10602 → 0x2E0421
0x10603 → 0x2E0359
0x10604 → 0x2E0445
0x10605 → 0x2E0361
(+ remaining codes registered in same init window)
```

**None** produced `HANDLER_ENTER` during init + 4 timer fire cycles.

## Interpretation

1. Launch param is **mapped into guest heap** but **not consumed** (no reg points to VA / no napptype string in r0–r3 under code observe).
2. Event handlers are **registered** but **never entered** under timer-only host loop → not the keepalive path.
3. Cmd dispatcher / cfg gate still **never entered** → confirms D4/D5.
4. Do **not** keep waiting on timer; next is D5c (only with DOCUMENTED/CROSS_TARGET event) or **D6 product track**.

## Note on bad seed

Early D5b build seeded handlers with RVA against a stale base (miss by `0x14`). Fixed: seed removed; rely on live `0x10102` only. Live correct handlers still had **0 enters**.
