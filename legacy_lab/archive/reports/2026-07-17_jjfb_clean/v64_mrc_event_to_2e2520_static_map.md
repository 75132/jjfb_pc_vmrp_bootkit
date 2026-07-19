# v64 mrc_event → 0x2E2520 Static Map

- MRP SHA256: `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- robotol.ext decompressed: 253420 bytes
- ext_base: `0x2D8DF4`

## Helper `0x304AED` → case 1 (`0x304B30`) → mrc_event

| step | VA | note |
|------|-----|------|
| helper entry | `0x304AED` | host `_mr_c_function_new` target |
| case 1 handler | `0x304B30` | `cmp r0,#8` → optional exit |
| **mrc_event** | `0x303E14` | BL from `0x304B42` |
| refresh | `0x30560C` | after mrc_event (case1 `0x304B4A`) |

### Case 1 handler

```text
0x00304B30: 6830
0x00304B32: 2808 cmp r0, #8
0x00304B34: D103 Bcond1 -> 0x304B3E
0x00304B36: BL 0x304480
0x00304B3A: 1C04
0x00304B3C: E01A B -> 0x304B74
0x00304B3E: 6871
0x00304B40: 68B2
0x00304B42: BL 0x303E14
0x00304B46: 1C04
0x00304B48: E014 B -> 0x304B74
0x00304B4A: BL 0x30560C
0x00304B4E: E011 B -> 0x304B74
0x00304B50: 60EF
0x00304B52: E00F B -> 0x304B74
0x00304B54: BL 0x304AD8
0x00304B58: E00C B -> 0x304B74
0x00304B5A: BL 0x3053B8
0x00304B5E: E009 B -> 0x304B74
0x00304B60: 4807 ldr r0, =0x13F0
0x00304B62: 4448 ADD/SUB reg
0x00304B64: 61C7
0x00304B66: E005 B -> 0x304B74
0x00304B68: 4805 ldr r0, =0x13F0
0x00304B6A: 4448 ADD/SUB reg
0x00304B6C: 6206
0x00304B6E: E001 B -> 0x304B74
0x00304B70: 4C04 ldr r4, =0x13
0x00304B72: 447C ADD/SUB reg
0x00304B74: 9900
0x00304B76: B005
0x00304B78: 1C20
0x00304B7A: 4689
0x00304B7C: BDF0 POP
```

Case 1 BL chain:
- `0x304B36` → `0x304480` mrc_exitApp?
- `0x304B42` → `0x303E14` mrc_event
- `0x304B4A` → `0x30560C` mrc_refreshScreenReal?
- `0x304B54` → `0x304AD8` 
- `0x304B5A` → `0x3053B8` 

## mrc_event @ `0x303E14`

```text
0x00303E14: B5F0 PUSH
0x00303E16: 1C15
0x00303E18: 4AEC ldr r2, =0xED8
0x00303E1A: 2701 movs r7, #1
0x00303E1C: 1C0C
0x00303E1E: 2806 cmp r0, #6
0x00303E20: B085
0x00303E22: 444A ADD/SUB reg
0x00303E24: D171 Bcond1 -> 0x303F0A
0x00303E26: 2900 cmp r1, #0
0x00303E28: D170 Bcond1 -> 0x303F0C
0x00303E2A: 4EE9 ldr r6, =0x7D8
0x00303E2C: 2400 movs r4, #0
0x00303E2E: 444E ADD/SUB reg
0x00303E30: 69B0
0x00303E32: 2800 cmp r0, #0
0x00303E34: D16B Bcond1 -> 0x303F0E
0x00303E36: 6810
0x00303E38: BL 0x303DE4
0x00303E3C: 4FE5 ldr r7, =0x8D8
0x00303E3E: 444F ADD/SUB reg
0x00303E40: 6378
0x00303E42: 20C8 movs r0, #200
0x00303E44: BL 0x2D99AC
0x00303E48: 61B0
0x00303E4A: 2100 movs r1, #0
0x00303E4C: 2200 movs r2, #0
0x00303E4E: 9202
0x00303E50: 9100
0x00303E52: 9101
0x00303E54: 6B79
0x00303E56: 2300 movs r3, #0
0x00303E58: 48DF ldr r0, =0x101AE
0x00303E5A: BL 0x304558
0x00303E5E: 6B78
0x00303E60: BL 0x3059E0
0x00303E64: 1C05
0x00303E66: 69B0
0x00303E68: 2800 cmp r0, #0
0x00303E6A: D00B Bcond0 -> 0x303E84
0x00303E6C: 6B78
0x00303E6E: 2800 cmp r0, #0
0x00303E70: D008 Bcond0 -> 0x303E84
0x00303E72: BL 0x3059E0
0x00303E76: 4BD9 ldr r3, =0x1420
0x00303E78: 1C82
0x00303E7A: 69B0
0x00303E7C: 6B79
0x00303E7E: 444B ADD/SUB reg
0x00303E80: 681B
0x00303E82: 4798 BX
0x00303E84: 2D00 cmp r5, #0
0x00303E86: DD4B Bcond13 -> 0x303F20
0x00303E88: 1E68
0x00303E8A: 213F movs r1, #63
0x00303E8C: 2200 movs r2, #0
0x00303E8E: 69B3
0x00303E90: 5D1F
0x00303E92: 2F00 cmp r7, #0
0x00303E94: D12E Bcond1 -> 0x303EF4
0x00303E96: 4284
0x00303E98: DA3F Bcond10 -> 0x303F1A
0x00303E9A: 191B
0x00303E9C: 785F
0x00303E9E: 2FA3 cmp r7, #163
0x00303EA0: D102 Bcond1 -> 0x303EA8
0x00303EA2: 7059
0x00303EA4: 3401
0x00303EA6: E025 B -> 0x303EF4
0x00303EA8: 2FA5 cmp r7, #165
0x00303EAA: D102 Bcond1 -> 0x303EB2
0x00303EAC: 7059
0x00303EAE: 3401
0x00303EB0: E020 B -> 0x303EF4
0x00303EB2: 2FA7 cmp r7, #167
0x00303EB4: D102 Bcond1 -> 0x303EBC
0x00303EB6: 7059
0x00303EB8: 3401
0x00303EBA: E01B B -> 0x303EF4
0x00303EBC: 2FA4 cmp r7, #164
0x00303EBE: D102 Bcond1 -> 0x303EC6
0x00303EC0: 7059
0x00303EC2: 3401
0x00303EC4: E016 B -> 0x303EF4
0x00303EC6: 2FD7 cmp r7, #215
0x00303EC8: D103 Bcond1 -> 0x303ED2
0x00303ECA: 272A movs r7, #42
0x00303ECC: 705F
0x00303ECE: 3401
0x00303ED0: E010 B -> 0x303EF4
0x00303ED2: 2FF7 cmp r7, #247
0x00303ED4: D103 Bcond1 -> 0x303EDE
0x00303ED6: 272F movs r7, #47
0x00303ED8: 705F
0x00303EDA: 3401
0x00303EDC: E00A B -> 0x303EF4
0x00303EDE: 2F5F cmp r7, #95
0x00303EE0: D103 Bcond1 -> 0x303EEA
0x00303EE2: 272D movs r7, #45
0x00303EE4: 705F
0x00303EE6: 3401
0x00303EE8: E004 B -> 0x303EF4
0x00303EEA: 2FB7 cmp r7, #183
0x00303EEC: D115 Bcond1 -> 0x303F1A
0x00303EEE: 272E movs r7, #46
0x00303EF0: 705F
0x00303EF2: 3401
0x00303EF4: 69B3
0x00303EF6: 5D1F
0x00303EF8: 2F20 cmp r7, #32
0x00303EFA: D10E Bcond1 -> 0x303F1A
0x00303EFC: 4284
0x00303EFE: DA0C Bcond10 -> 0x303F1A
0x00303F00: 191F
0x00303F02: 787F
0x00303F04: 2FAC cmp r7, #172
0x00303F06: D108 Bcond1 -> 0x303F1A
0x00303F08: E002 B -> 0x303F10
0x00303F0A: E020 B -> 0x303F4E
0x00303F0C: E009 B -> 0x303F22
0x00303F0E: E072 B -> 0x303FF6
0x00303F10: 551A
0x00303F12: 69B3
0x00303F14: 191B
0x00303F16: 7059
```

| r0 (event) | static path | reaches 0x2E2520? |
|-----------:|-------------|-------------------|
| 0 | `303E36` dialog/key init via `304558` indirect | no |
| 1–5 | `cmp r0,#0` → `B 303F0E` → `303FF6` `movs r0,#0; POP` | **no (immediate return 0)** |
| 6+ | `cmp r0,#6` → `303F4E` sub-switch (0/1/2/20/58…) | no |
| 8 | helper case1 also calls `304480` (exit) before return | no |

Static proof: `mrc_event` has **zero** BL to `0x2E2520`, `0x2DC80C`, `0x312AC4`, `0x312C0C`.

### mrc_event BL callees (first 16)
- `0x303E38` → `0x303DE4`
- `0x303E44` → `0x2D99AC`
- `0x303E5A` → `0x304558`
- `0x303E60` → `0x3059E0`
- `0x303E72` → `0x3059E0`
- `0x303F26` → `0x3132D8`
- `0x303F44` → `0x31027C`
- `0x303F48` → `0x31271C`
- `0x303F54` → `0x300114`
- `0x303F60` → `0x301848`
- `0x303F7A` → `0x312898`
- `0x303F86` → `0x2E419C`
- `0x303FAA` → `0x304558`
- `0x303FC8` → `0x30C008`
- `0x303FCC` → `0x30E55C`
- `0x303FF0` → `0x304558`

## Jump table `0x2E2520` (index = event_code − 3)

| event | name | dest | note |
|------:|------|------|------|
| 3 | `MR_MOUSE_UP` | `0x2E379E` | → 2DC4D8 (B71) |
| 4 | `MR_MENU_SELECT` | `0x2E4194` |  |
| 5 | `MR_MENU_RETURN` | `0x2E4040` | → 2E4066 → 2DADC4 (Path A) |
| 6 | `MR_DIALOG_EVENT` | `0x2E406C` | → 2E4066 → 2DADC4 (Path A) |
| 7 | `MR_SMS_INDICATION` | `0x2E4074` |  |
| 8 | `MR_EXIT_EVENT` | `0x2E40CE` |  |
| 9 | `MR_SMS_RESULT` | `0x2E407C` |  |
| 10 | `MR_LOCALUI_EVENT` | `0x2E40CE` |  |
| 11 | `MR_OSD_EVENT` | `0x2E408C` |  |
| 12 | `MR_MOUSE_MOVE` | `0x2E4040` | → 2E4066 → 2DADC4 (Path A) |
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

## ALL callers / branches → `0x2E2520`

**Only 2 direct BL sites in entire robotol.ext:**
- `0x2DC8D4` (inside queue-drain mega-fn)
- `0x2E7B9E` (inside 2E7B7C loop)

B/Bcond direct to `0x2E2520`: 0

## Queue drain: timer → `0x2DC80C` → fall-through `0x2DC8D4` → `0x2E2520`

`0x2DC80C` is a **single mega-function** (not a tiny wrapper). Early exit at `0x2DC846` when queue helpers `0x312AC4`/`0x312AB4` report empty (`cmp r0,#0`). When a queued event survives filtering, control reaches `0x2DC8D2` → **`0x2DC8D4` BL `0x2E2520`**.

```text
0x002DC80C: B5F8 PUSH
0x002DC80E: 4C55 ldr r4, =0xB54
0x002DC810: 444C ADD/SUB reg
0x002DC812: 6820
0x002DC814: BL 0x312AC4
0x002DC818: 2500 movs r5, #0
0x002DC81A: 43EF
0x002DC81C: 1C06
0x002DC81E: 2800 cmp r0, #0
0x002DC820: DD11 Bcond13 -> 0x2DC846
0x002DC822: 4850 ldr r0, =0xB54
0x002DC824: 2100 movs r1, #0
0x002DC826: 4448 ADD/SUB reg
0x002DC828: 6800
0x002DC82A: BL 0x312AB4
0x002DC82E: 1C04
0x002DC830: D10A Bcond1 -> 0x2DC848
0x002DC832: 4C4C ldr r4, =0xB54
0x002DC834: 2100 movs r1, #0
0x002DC836: 444C ADD/SUB reg
0x002DC838: 6820
0x002DC83A: BL 0x312C0C
0x002DC83E: 494A ldr r1, =0xB7D
0x002DC840: 2001 movs r0, #1
0x002DC842: 4449 ADD/SUB reg
0x002DC844: 7008
0x002DC846: BDF8 POP
0x002DC848: 4848 ldr r0, =0xC76
0x002DC84A: 2300 movs r3, #0
0x002DC84C: 4448 ADD/SUB reg
0x002DC84E: 56C0
0x002DC850: 2801 cmp r0, #1
0x002DC852: D112 Bcond1 -> 0x2DC87A
0x002DC854: 6820
0x002DC856: 2850 cmp r0, #80
0x002DC858: D00F Bcond0 -> 0x2DC87A
0x002DC85A: 1C20
0x002DC85C: BL 0x30BC40
0x002DC860: 4840 ldr r0, =0xB54
0x002DC862: 2100 movs r1, #0
0x002DC864: 4448 ADD/SUB reg
0x002DC866: 6800
0x002DC868: BL 0x312C0C
0x002DC86C: 3D01
0x002DC86E: 493E ldr r1, =0xB7D
0x002DC870: 3E01
0x002DC872: 2001 movs r0, #1
0x002DC874: 4449 ADD/SUB reg
0x002DC876: 7008
0x002DC878: E06E B -> 0x2DC958
0x002DC87A: 483D ldr r0, =0xED8
0x002DC87C: 4448 ADD/SUB reg
0x002DC87E: 6800
0x002DC880: 2800 cmp r0, #0
0x002DC882: DD26 Bcond13 -> 0x2DC8D2
0x002DC8D4: BL 0x2E2520
0x002DC8D8: 2801 cmp r0, #1
0x002DC8DA: D0B4 Bcond0 -> 0x2DC846
0x002DC8DC: 4821 ldr r0, =0xB54
0x002DC8DE: 2100 movs r1, #0
0x002DC8E0: 4448 ADD/SUB reg
0x002DC8E2: 6800
0x002DC8E4: BL 0x312C0C
0x002DC8E8: 1C20
0x002DC8EA: BL 0x30BC40
```

- Sole BL→`0x2DC80C`: **`0x305EBE`** (inside callback `0x305EB8`)
- `0x305EB8` has **no** direct BL callers → registered callback / timer (`dealtimer`)
- v63 dynamic: `2DC80C` hit hundreds of times but queue always empty → never reaches `2DC8D4`

## Alternate dispatch: `0x2E7B7C` → `0x2E7B9E` → `0x2E2520`

### `0x2E7B7C`

- Direct **BL** callers (2): `0x2F4F7E`, `0x313320`
- Direct **B/Bcond** to entry (0): (none)

```text
0x002E7B7C: B5F8 PUSH
0x002E7B7E: 4F0F ldr r7, =0xD6C
0x002E7B80: 444F ADD/SUB reg
0x002E7B82: 68B8
0x002E7B84: 2800 cmp r0, #0
0x002E7B86: D018 Bcond0 -> 0x2E7BBA
0x002E7B88: BL 0x312AC4
0x002E7B8C: 1C04
0x002E7B8E: 2800 cmp r0, #0
0x002E7B90: DD13 Bcond13 -> 0x2E7BBA
0x002E7B92: 2500 movs r5, #0
0x002E7B94: 2100 movs r1, #0
0x002E7B96: 68B8
0x002E7B98: BL 0x312AB4
0x002E7B9C: 1C06
0x002E7B9E: BL 0x2E2520
0x002E7BA2: 2801 cmp r0, #1
0x002E7BA4: D009 Bcond0 -> 0x2E7BBA
0x002E7BA6: 1C30
0x002E7BA8: BL 0x30BC40
0x002E7BAC: 2100 movs r1, #0
0x002E7BAE: 68B8
0x002E7BB0: BL 0x312C0C
0x002E7BB4: 3C01
0x002E7BB6: 42A5
0x002E7BB8: DBEC Bcond11 -> 0x2E7B94
0x002E7BBA: BDF8 POP
0x002E7BBC: 0D6C
0x002E7BBE: 0000
0x002E7BC0: B5F8 PUSH
0x002E7BC2: 4F12 ldr r7, =0xB5C
0x002E7BC4: 1C04
0x002E7BC6: 6840
0x002E7BC8: 4E11 ldr r6, =0xB60
0x002E7BCA: 444F ADD/SUB reg
0x002E7BCC: 6038
0x002E7BCE: 2100 movs r1, #0
0x002E7BD0: 444E ADD/SUB reg
0x002E7BD2: 6031
0x002E7BD4: 2500 movs r5, #0
0x002E7BD6: 43ED
0x002E7BD8: 1C31
0x002E7BDA: 68A2
0x002E7BDC: BL 0x308DE8
0x002E7BE0: 42A8
0x002E7BE2: D00E Bcond0 -> 0x2E7C02
0x002E7BE4: 68A2
0x002E7BE6: 6838
0x002E7BE8: 1C31
0x002E7BEA: BL 0x308DE8
0x002E7BEE: 1C05
0x002E7BF0: 68A2
0x002E7BF2: 6838
0x002E7BF4: 1C31
0x002E7BF6: BL 0x308DE8
0x002E7BFA: 4906 ldr r1, =0xA90
0x002E7BFC: 4449 ADD/SUB reg
0x002E7BFE: 700D
0x002E7C00: BDF8 POP
0x002E7C02: 4904 ldr r1, =0xA90
0x002E7C04: 4449 ADD/SUB reg
0x002E7C06: 700D
0x002E7C08: E7FA B -> 0x2E7C00
0x002E7C0A: 0000
0x002E7C0C: 0B5C
0x002E7C0E: 0000
0x002E7C10: 0B60
0x002E7C12: 0000
0x002E7C14: 0A90
0x002E7C16: 0000
0x002E7C18: B5F8 PUSH
0x002E7C1A: 1C05
0x002E7C1C: 491D ldr r1, =0xB5C
0x002E7C1E: 6840
0x002E7C20: 4449 ADD/SUB reg
0x002E7C22: 6008
0x002E7C24: 491C ldr r1, =0xB60
0x002E7C26: 2400 movs r4, #0
0x002E7C28: 2000 movs r0, #0
0x002E7C2A: 4449 ADD/SUB reg
0x002E7C2C: 6008
0x002E7C2E: 68A8
```
- Upstream BL→`0x2F4F7E`: (none / fall-through)
- Upstream BL→`0x313320`: (none / fall-through)

```text
0x002E7B9E: BL 0x2E2520
0x002E7BA2: 2801 cmp r0, #1
0x002E7BA4: D009 Bcond0 -> 0x2E7BBA
0x002E7BA6: 1C30
0x002E7BA8: BL 0x30BC40
0x002E7BAC: 2100 movs r1, #0
0x002E7BAE: 68B8
0x002E7BB0: BL 0x312C0C
0x002E7BB4: 3C01
0x002E7BB6: 42A5
0x002E7BB8: DBEC Bcond11 -> 0x2E7B94
```

BL→`0x2E7B7C`: `0x2F4F7E`, `0x313320`
- Upstream BL→`0x2F4F7E`: (none / table dispatch)
- Upstream BL→`0x313320`: (none / table dispatch)

## Path A stub (event 5 / 12)

```text
0x002E4040: 493B ldr r1, =0xB5C
0x002E4042: 6860
0x002E4044: 4449 ADD/SUB reg
0x002E4046: 6008
0x002E4048: 493A ldr r1, =0xB60
0x002E404A: 4D47 ldr r5, =0xA90
0x002E404C: 2000 movs r0, #0
0x002E404E: 4449 ADD/SUB reg
0x002E4050: 6008
0x002E4052: 444D ADD/SUB reg
0x002E4054: 63A8
0x002E4056: 4845 ldr r0, =0xB58
0x002E4058: 1C21
0x002E405A: 4448 ADD/SUB reg
0x002E405C: 6800
0x002E405E: BL 0x2F68E4
0x002E4062: 63A8
0x002E4064: 60A8
0x002E4066: BL 0x2DADC4
0x002E406A: E093 B -> 0x2E4194
0x002E406C: 1C20
0x002E406E: BL 0x2DEB3C
```

## Conclusion

### Why helper(code=1) + mrc_event(5) returns 0 but never hits `0x2E2520`

1. **Two different event layers.** Helper code=1 calls platform **`mrc_event@0x303E14`** (Mythroad API). The Path-A switch **`0x2E2520`** is robotol **internal** dispatch, reached only from **`0x2DC8D4`** or **`0x2E7B9E`**.
2. **`mrc_event(5,0,0)` is a no-op.** For `1 ≤ r0 ≤ 5`, static flow is: `cmp r0,#0` → branch to `0x303F0E` → `0x303FF6` → `movs r0,#0; POP`. No enqueue, no BL to queue fns, no call to `0x2E2520`. Return 0 is expected.
3. **`0x2E2520` has exactly 2 BL callers** in all of robotol.ext: `0x2DC8D4` (queue-drain tail) and `0x2E7B9E` (secondary loop). Neither is reachable from `mrc_event`.
4. **Queue drain requires a pre-filled queue.** Timer/callback path `dealtimer → 0x305EB8 → 0x305EBE → 0x2DC80C` runs, but if nothing was enqueued (via a different producer than helper code=1), it exits early at `0x2DC846` — matching v63 logs (`2DC80C` hit, `2E2520`=0).

### Correct host delivery contract for Path A (`0x2E4040` → `0x2DADC4`)

**Path A cannot be reached via helper mrc_event alone.** Required contract:

1. **Enqueue** event code **5** (`MR_MENU_RETURN`) or **12** (`MR_MOUSE_MOVE`) into the robotol event queue consumed by `0x312AC4`/`0x312AB4`/`0x312C0C` at the start of `0x2DC80C` (producer is **not** `mrc_event@0x303E14`; likely Mythroad `sendAppEvent` / ext event callback / indirect `0x304558`-style dispatch).
2. **Drain** via existing timer contract: host helper **code=2** (`mrc_timerTimeout`) → `dealtimer` callback → `0x305EB8` → `0x305EBE` → `0x2DC80C` → (non-empty queue) → `0x2DC8D4` → `0x2E2520` → stub `0x2E4040` → **`BL 0x2DADC4`**.
3. Alternate route: invoke the **`0x2E7B7C`** loop (callers `0x2F4F7E`, `0x313320`) which tail-calls `0x2E7B9E` → `0x2E2520` — still requires the queue/state that loop expects.

### Key addresses

| role | VA |
|------|-----|
| robotol helper | `0x304AED` |
| helper case1 | `0x304B30` |
| platform mrc_event (NOT internal switch) | `0x303E14` |
| queue-drain entry | `0x2DC80C` |
| BL→2E2520 (queue path) | `0x2DC8D4` |
| BL→2E2520 (alt loop) | `0x2E7B9E` |
| internal event switch | `0x2E2520` |
| Path A stub (ev 5/12) | `0x2E4040` |
| Path A BL gate_init | `0x2E4066` |
| gate_init target | `0x2DADC4` |
| timer callback | `0x305EB8` |
| callback queue drain call | `0x305EBE` |