# Stage E9U Object/Count Verdict

- **Case**: tick_3124D8
- **Mode**: tick (`JJFB_FAST_PROGRESS_TICK_CALL=1`, `JJFB_E9U_TICK_N=4`)
- **Class**: `SPLASH_PROGRESS_VISIBLE_WITH_REAL_TICK`
- **Evidence**: OBSERVED
- **Elapsed**: 94.2s
- **Product success**: **NO** (`NOT_PRODUCT_SUCCESS`)

## 1. Verdict ladder (what this stage claims)

| Class | Hit? |
|-------|------|
| `PROGRESS_COUNT_REAL_TICK_RESTORED` | Yes (count 0→4 via real STR) |
| `PROGRESS_COUNT_REAL_CALLER_RESTORED` | Yes (compat tag) |
| `SPLASH_PROGRESS_VISIBLE_WITH_REAL_TICK` | Yes (progress blit + UI) |
| `PROGRESS_TICK_GATE_BLOCKS` | No (`gate8B8=0`) |
| `BD0_UPSTREAM_CALLER_REQUIRES_OBJECT` | Yes (副目标仍未解) |
| Direct `BA0+0x2C` poke as success | **Forbidden / not used** |
| `FAST_SPLASH_PROGRESS_OBJECT_ASSIST` poke | **Off** (`assist=0`) |

## 2. Primary result — progress count via real tick

Natural writer (static + live):

```text
0x3124D8  progress tick
  LDR [R9+0x8B8] → BLX gate; CMP #0
  if gate==0:
    r4 = R9+BA0
    [r4+#0x2C] += 1          ; STR @ 0x3124FE
    if count > 12: count = 0 ; STR @ 0x312504
Natural caller: 0x2F55FA (timer path) — NOT reached on FAST splash
Assist: JJFB_FAST_PROGRESS_TICK_CALL → call real 0x3124D8 ×4
```

Live tick sequence:

| i | count before | count after | STR@3124FE | end |
|---|--------------|-------------|------------|-----|
| 1 | 0 | 1 | hit | stop_at_base |
| 2 | 1 | 2 | hit | stop_at_base |
| 3 | 2 | 3 | hit | stop_at_base |
| 4 | 3 | **4** | hit | stop_at_base |

Log excerpt:

```text
[JJFB_FAST_PROGRESS_TICK_CALL] entry=0x3124D8 n=4 gate8B8=0x0 count_before=0 BD0=0x2A8364
[JJFB_FAST_PROGRESS_TICK_DONE] i=1/4 count=0->1
[JJFB_FAST_PROGRESS_TICK_DONE] i=2/4 count=1->2
[JJFB_FAST_PROGRESS_TICK_DONE] i=3/4 count=2->3
[JJFB_FAST_PROGRESS_TICK_DONE] i=4/4 count=3->4
[JJFB_E9U_CLASS] class=PROGRESS_COUNT_REAL_TICK_RESTORED count=4 writer=0x3124FE note=no_direct_BA0_plus2C_poke
```

## 3. Visible UI (OBSERVED)

| Item | Result |
|------|--------|
| loadingbar | True (`SPLASH_LOADING_UI_VISIBLE` mask→0xB) |
| progress segments | True ×4 (`bar!16!18.bmp` x=47,59,71,83 y=226) |
| BD0 status string | `0x2A8364` (real `0x2FC418` → `STR@0x2FC444`) |
| `0x12340` measure | True (`PLATFORM_TEXT_MEASURE_12340_RENDERED`, w=72 h=18) |
| `0x11F00` draw | True (`PLATFORM_TEXT_API_11F00_RENDERED` @111,261) |
| transparent text | True (`PLATFORM_TEXT_TRANSPARENT_RENDERED`) |
| request rewrite | Off |
| REAL_MRP_MEMBER_BRIDGE success | Not claimed |
| invent pixels / fake DRAW | No |

Progress blit samples:

```text
SPLASH_PROGRESS_DRAWN_NO_REWRITE member=bar!16!18.bmp x=47 y=226
SPLASH_PROGRESS_DRAWN_NO_REWRITE member=bar!16!18.bmp x=59 y=226
SPLASH_PROGRESS_DRAWN_NO_REWRITE member=bar!16!18.bmp x=71 y=226
SPLASH_PROGRESS_DRAWN_NO_REWRITE member=bar!16!18.bmp x=83 y=226
```

Splash slots at draw:

```text
BA0+0x20 bar=0x3910080
BA0+0x24 textbar=0x39100C0
BA0+0x28 loadingbar=0x2A83D4
BA0+0x2C count=4
R9+BD0=0x2A8364
```

## 4. BD0 path (carried from E9S; unchanged)

```text
JJFB_FAST_BD0_INIT_CALL=1
0x2FC418(r0=0x3146C4 "连接中，请稍候")
  → 0x2D9648 concat
  → STR @0x2FC444 → BA0+0x30 / R9+BD0 = 0x2A8364
  → ui_mode stays/writes 0x45
BD0: 0x0 → 0x2A8364  (real STR, no direct poke)
```

## 5. Secondary — 2FC03C object slot (still blocked)

| Field | Value |
|-------|-------|
| slot | `R9+0x11EC+0x24` = `0x2B2A68` |
| w0..w3 | all `0` |
| Class | `BD0_UPSTREAM_CALLER_REQUIRES_OBJECT` |
| Natural filler | `0x2FEBBC` (STM/`STR r5,[r0,#0x24]`) — **not hit** |
| Callers of `2FC03C` | `0x2DAE24`, `0x2FECAA` — **not reached** |
| Implication | `JJFB_FAST_BD0_CALLER_2FC03C` still unsafe; keep bare `0x2FC418` |

Object CSV:

```text
phase,obj_base,w0,w1,w2,w3,gate8b8,count,bd0
pre_splash,0x2B2A68,0,0,0,0,0,0,0x2A8364
before_tick,0x2B2A68,0,0,0,0,0,0,0x2A8364
after_tick,0x2B2A68,0,0,0,0,0,4,0x2A8364
```

## 6. Assists still on (honest remaining)

Still required on this visible path:

1. `JJFB_FAST_BD0_INIT_CALL` (real `0x2FC418`) — object upstream not ready
2. `JJFB_FAST_PROGRESS_TICK_CALL` (real `0x3124D8` ×N) — timer `0x2F55FA` not reached
3. `JJFB_FAST_SPLASH_CALL` + DisplayFirst path
4. UI_MODE / C9D branch assists (E9T case3 honesty: splash is force-called)

Removed / not used as success:

- direct BD0 poke
- direct `BA0+0x2C` poke
- `FAST_SPLASH_PROGRESS_OBJECT_ASSIST`

## 7. Remaining next knives (NOT product)

1. Natural timer reach `0x2F55FA` → drop `FAST_PROGRESS_TICK_CALL`
2. Live `0x2FEBBC` object → safe `0x2FC03C` → drop bare `0x2FC418` assist
3. True DisplayFirst / C9D / idle without `FAST_SPLASH_CALL`

## 8. Artifacts

| Kind | Path |
|------|------|
| Verdict | `reports/stage_e9u_object_count_verdict.md` |
| Log | `logs/e9u_object_count_stdout.txt` |
| Count CSV | `reports/e9u_count_writer_trace.csv` |
| Object CSV | `reports/e9u_object_slot_trace.csv` |
| HWND shot | `screenshots/e9u_actual_window_capture.png` |
| UI shot | `screenshots/e9u_splash_visible.png` |
| Decode | `out/e9u_tmp/e9u_decode.txt` |
| Runner | `RUN_E9U_OBJECT_COUNT.ps1 -Mode tick\|object\|trace` |

## 9. How to re-run

```powershell
.\RUN_E9U_OBJECT_COUNT.ps1 -Mode tick -Seconds 90 -HoldSec 12 -TickN 4
# object probe only (no tick):
.\RUN_E9U_OBJECT_COUNT.ps1 -Mode object -SkipBuild
# trace probe only:
.\RUN_E9U_OBJECT_COUNT.ps1 -Mode trace -SkipBuild
```

Env (success path):

```text
JJFB_E9U_MODE=1
JJFB_FAST_PROGRESS_TICK_CALL=1
JJFB_E9U_TICK_N=4
JJFB_FAST_BD0_INIT_CALL=1
JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST=0   # must stay off
JJFB_DEBUG_PROGRESS_COUNT_POKE             # never as verdict success
```
