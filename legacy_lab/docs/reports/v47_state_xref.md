# v47 state xref — ui_mode / AC8 / progress_count

## Targets

| Slot | Offset | Addr (ERW=0x2B1850) | Role |
|------|--------|---------------------|------|
| ui_mode | +0x8D0 | 0x2B2120 | splash select (==0x45) |
| AC8 | +0xAC8 | 0x2B2318 | slogo (>0) / loading (==0) |
| progress_count | +0xBA0+0x2C | 0x2B241C | bar idx gate |
| bar_obj | +0xBA0+0x20 | 0x2B2410 | written |
| textbar_obj | +0xBA0+0x24 | 0x2B2414 | written |
| loadingbar_obj | +0xBA0+0x28 | 0x2B2418 | written |

---

## Dynamic writes (Run A, driver=off)

### Executed writes in ERW+0x800..0xC50

| off | tag | pc | lr | notes |
|-----|-----|----|----|-------|
| 0xBC8 | loadingbar_obj | **0x2EFA34** | 0x304601 | first splash |
| 0xBC0 | bar_obj | **0x2EFA44** | 0x304601 | first splash |
| 0xBC4 | textbar_obj | **0x2EFA52** | 0x304601 | first splash |
| 0xA58..0xA88, 0x8A4.. | chrome-ish obj slots | 0x2F44C0.. | 0x304601 | tick 11 |
| 0xC4C / 0xC50 | timers? | 0x2E87EA/F0 | 0x2E87C3 | increments |

### Never written by guest

| off | tag | executed? |
|-----|-----|-----------|
| 0xBCC | **progress_count** | **NO** |
| 0xAC8 | **AC8** | **NO** |
| 0x8D0 | ui_mode | only host FORCE |

Splash binds bar/textbar/loadingbar objects at `0x2EFA34/44/52` but **never stores +0x2C**.

---

## Static literals (guest code scan)

| name | hits | sample sites |
|------|------|----------------|
| imm_8D0 | 14 | 0x2EFF1C, 0x2F0304, 0x306CA8, … |
| imm_AC8 | **1** | **0x2EFC6C** (splash neighborhood) |
| imm_BA0 | **2** | **0x2EFC58**, 0x312588 |
| imm_2C | 1 | 0x31394C (string area; weak) |
| str_check_update etc. | **0** | VA words not in scanned bands |

**Candidate writer neighborhood:** literals `0xBA0` @ `0x2EFC58` and `0xAC8` @ `0x2EFC6C` sit just after splash (`0x2EF86C`..`0x2EFADE`).  
Likely same function / trailing helper that *can* touch AC8/BA0 — but the **progress_count store path is not taken** in current runs.

---

## Reads (known)

| target | pc | executed? |
|--------|-----|-----------|
| AC8 | 0x2EF8A0 | YES (always 0) |
| progress_count | 0x2EFAA0 | YES (once, value 0 unless driver) |
| check_update strings | — | **NO** |

---

## Conclusion

- Object slots writers: **found and executed** (`0x2EFA34/44/52`).
- progress_count / AC8 writers: **literal sites known; store never executes**.
- Next: disassemble `0x2EFC40..0x2EFD00` and xref who should branch into the STR to `BA0+0x2C`.
