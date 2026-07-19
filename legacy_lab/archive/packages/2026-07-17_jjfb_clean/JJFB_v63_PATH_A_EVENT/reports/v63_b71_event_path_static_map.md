# v63 B71 / Event Path Static Map

## Jump table `0x2E2520` (index = event_code - 3)

| event | name | dest | note |
|------:|------|------|------|
| 3 | `MR_MOUSE_UP` | `0x2E379E` | → BL 2DC4D8 (B71/15D) |
| 4 | `MR_MENU_SELECT` | `0x2E4194` |  |
| 5 | `MR_MENU_RETURN` | `0x2E4040` |  |
| 6 | `MR_DIALOG_EVENT` | `0x2E406C` | → BL 2DADC4 (Path A gate) |
| 7 | `MR_SMS_INDICATION` | `0x2E4074` |  |
| 8 | `MR_EXIT_EVENT` | `0x2E40CE` |  |
| 9 | `MR_SMS_RESULT` | `0x2E407C` |  |
| 10 | `MR_LOCALUI_EVENT` | `0x2E40CE` |  |
| 11 | `MR_OSD_EVENT` | `0x2E408C` |  |
| 12 | `MR_MOUSE_MOVE` | `0x2E4040` |  |
| 13 | `MR_ERROR_EVENT` | `0x2E4194` |  |
| 14 | `?` | `0x2E4028` |  |
| 15 | `?` | `0x2E4020` |  |
| 16 | `?` | `0x2E4008` |  |
| 17 | `?` | `0x2E3FF0` |  |
| 18 | `?` | `0x2E3FD8` |  |
| 19 | `?` | `0x2E3FD0` |  |
| 20 | `?` | `0x2E3FB2` |  |
| 21 | `?` | `0x2E3F9A` |  |
| 22 | `?` | `0x2E3F82` |  |

## Key correction vs v62 candidate

- `MR_MOUSE_UP(3)` → `2DC4D8`.
- When `15D==1`, that path sets **`134D=2` and `B71=1`** together.
- `305EB8` requires `134D==0`, so this path **cannot** open Path C by itself.
- When `15D!=1`, it only sets `15D=1` (no B71).
- Therefore MOUSE_UP is **not** the bootstrap for `305EB8→2DADC4`.

## Path A (bypass 305EB8 gates)

- `MR_MENU_RETURN(5)` / `MR_MOUSE_MOVE(12)` → `2E4066` → `2DADC4`.
- This enters gate_init **without** needing B71.
- After `2DADC4`, tail `2DAE72` calls `30ED2C(r1=1)` which can set B71 for later Path C ticks.

## 2DC4D8

```text
0x002DC4D8: B5F8 PUSH
0x002DC4DA: 1C06
0x002DC4DC: 4933 ldr r1, =0xB5C
0x002DC4DE: 6840
0x002DC4E0: 4F33 ldr r7, =0xB60
0x002DC4E2: 4449 ADD ERW
0x002DC4E4: 6008
0x002DC4E6: 2100 movs r1, #0
0x002DC4E8: 444F ADD ERW
0x002DC4EA: 6039
0x002DC4EC: 68B2
0x002DC4EE: 1C39
0x002DC4F0: BL 0x308D98
0x002DC4F4: 1C04
0x002DC4F6: 2101 movs r1, #1
0x002DC4F8: BL 0x305E30
0x002DC4FC: 1C05
0x002DC4FE: 1C03
0x002DC500: 482A ldr r0, =0xB5C
0x002DC502: 68B2
0x002DC504: 4448 ADD ERW
0x002DC506: 6800
0x002DC508: 1C39
0x002DC50A: BL 0x307FBC
0x002DC50E: 4829 ldr r0, =0xB6C
0x002DC510: 4448 ADD ERW
0x002DC512: 6801
0x002DC514: 2900 cmp r1, #0
0x002DC516: D044 Bcond0 -> 0x2DC5A2
0x002DC518: 4827 ldr r0, =0x820
0x002DC51A: 4448 ADD ERW
0x002DC51C: 6800
0x002DC51E: BL 0x2D9648
0x002DC522: 1C07
0x002DC524: 210C movs r1, #12
0x002DC526: BL 0x304AC4
0x002DC52A: 1C06
0x002DC52C: 1C38
0x002DC52E: BL 0x305E08
0x002DC532: 4F22 ldr r7, =0x15C
0x002DC534: 2301 movs r3, #1
0x002DC536: 444F ADD ERW
0x002DC538: 56F8 ldrsb r0,[r7,r3]
0x002DC53A: 2801 cmp r0, #1
0x002DC53C: D104 Bcond1 -> 0x2DC548
0x002DC53E: 4920 ldr r1, =0xB88
0x002DC540: 4449 ADD ERW
0x002DC542: 6808
0x002DC544: 1900
0x002DC546: 6008
0x002DC548: 2202 movs r2, #2
0x002DC54A: 2100 movs r1, #0
0x002DC54C: 1C30
0x002DC54E: BL 0x3053D0
0x002DC552: 1C22
0x002DC554: 1C29
0x002DC556: 1C30
0x002DC558: BL 0x3059CC
0x002DC55C: 2301 movs r3, #1
0x002DC55E: 56F8 ldrsb r0,[r7,r3]
0x002DC560: 2101 movs r1, #1
0x002DC562: 2801 cmp r0, #1
```

## 2E4066

```text
0x002E4060: FC41
0x002E4062: 63A8
0x002E4064: 60A8
0x002E4066: BL 0x2DADC4
0x002E406A: E093 B -> 0x2E4194
0x002E406C: 1C20
0x002E406E: BL 0x2DEB3C
0x002E4072: E08F B -> 0x2E4194
0x002E4074: 1C20
0x002E4076: BL 0x2DE7BC
0x002E407A: E08B B -> 0x2E4194
0x002E407C: 1C20
0x002E407E: BL 0x2DFAD4
0x002E4082: E087 B -> 0x2E4194
0x002E4084: 1C20
0x002E4086: BL 0x2DDD10
0x002E408A: E083 B -> 0x2E4194
0x002E408C: 4928 ldr r1, =0xB5C
0x002E408E: 6860
```

## Dynamic next step

Observe whether host ever delivers event 5/12 into robotol queue,
and whether a single controlled Path-A probe (opt-in env) reaches 2DADC4.
No FORCE ui_mode / C0 inject / host UI / event-code blind scan.

## v63 dynamic result

- `JJFB_PATH_A_EVENT_ONCE=5` fired; `mrc_event` via **helper code=1** returned 0.
- `2DC80C` drained hundreds of times; **`V56_EVENT` / `2E2520` / `2E4066` = 0**.
- Helper code=1 jump lands at `0x304B30` → `0x303E14` (not event-queue → `2E2520`).
- `0x2DC8D4` has **no direct BL callers** (likely function-pointer / table dispatch).

Next: find the real enqueue / call site into `2E2520`.
