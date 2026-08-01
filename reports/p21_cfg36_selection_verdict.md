# P21 cfg36 Selection Verdict

## Bottom line

**Class: A — no real cfg file / list read**

P20-CLEAN freeze held. Lane A (no fast assist) and Lane B (assisted) both enter gamelist, establish isolated ERW, and idle on natural FIRE_EXT without ever opening `cfg.bin`. First failing gate is `CFG_FILE_OPENED`.

Primary evidence: **Lane A** (`research_assisted=no`, `product_valid=yes`).

## Evidence tiers

| Lane | FAST_BD0 / PROGRESS | gamelist | ERW iso | FIRE_EXT | cfg_open | cfg36_present | cfg36_selected |
|------|---------------------|----------|---------|----------|----------|---------------|----------------|
| A natural | 0 / 0 | YES | YES | 3+ | NO | NO | NO |
| B assisted | 1 / 1 | YES | YES | 3+ | NO | NO | NO |

Same cfg stop A/B: **YES** — fast assist is not required to reach the gamelist / cfg-stop ceiling.

## Five gates (must not collapse)

| Gate | Pass | Note |
|------|------|------|
| CFG_FMT_MAPPED | YES | `va=0x2E848C` format string only — **not** selection |
| CFG_FILE_OPENED | **NO** | no VFS open / no `plat_10112` for `cfg.bin` |
| CFG_RECORD_READ | NO | blocked by open |
| CFG36_RECORD_PRESENT | NO | no guest buffer with full signature |
| CFG36_SELECTED | NO | no Guest selected-state write |

## P20 freeze checks

| Check | Result |
|-------|--------|
| gbrwcore entry | PASS |
| br_exit CONTINUE | PASS |
| gamelist entered | PASS |
| gamelist ERW isolated | PASS (`P=0x2AC8EC` `erw=0x682B8C`) |
| 0x30D5D2 fault | 0 |
| forced 10140 | 0 |
| MRPGCMAP EMU_OK (defer+drain) | PASS |

## Host-side cfg data (offline; not guest-loaded)

| Source | Size | cfg36 possible? |
|--------|------|-----------------|
| `gamelist.mrp` member `cfg.bin` | 6898 | **NO** — max ~21 records; index 36 needs offset 10816 |
| `game_files/.../320x480/gwy/cfg.bin` | 20728 | **YES** — offline inspect: napptype=12 nextid=482 ncode=512 narg=0 narg1=1 target=`gwy/jjfb.mrp` |

So the **real list for cfg36 is the external SHARED_ROOT file**, but Guest never issued the open/10112 that would load it.

## File I/O ledger (gamelist window)

Observed opens/reads under P21 filter: font `gb16.uc2`, package `gwy/gamelist.mrp` member table / gzip payloads. **Zero** `cfg.bin` / `gwy/cfg.bin` / `dsm.cfg` paths.

First timer FIRE_EXT saw those package/font calls (`file_activity`); subsequent fires were `idle_or_ui_refresh` with no file/cfg/state change — do not extend run time solely for more timer fires.

## Launch-param provenance

| Field | Raw offset | Parse PC | Module | Parsed | Write VA | Gamelist read |
|-------|------------|----------|--------|--------|----------|---------------|
| napptype | 0 | 0x9B8C8 | cfunction.ext | 12 | (unset) | YES @ 0x2E77AE |
| nextid | 12 | 0x9B8C8 | cfunction.ext | 482 | (unset) | YES @ 0x2E77AE |
| ncode | 23 | 0x9B8C8 | cfunction.ext | 512 | (unset) | not observed as consumer |
| narg | 33 | 0x9B8C8 | cfunction.ext | 0 | (unset) | not observed |
| narg1 | 40 | 0x9B8C8 | cfunction.ext | 1 | (unset) | not observed |
| nmrpname | 48 | 0x9B8C8 | cfunction.ext | gwy/jjfb.mrp | (unset) | not observed |
| gwyblink | 70 | 0x9B8C8 | cfunction.ext | present | (unset) | not observed as gamelist consumer |

Verdict on handoff:

- cfunction **does** byte-read the mapped entry string (param VA `0x682AEC` after gamelist start_dsm).
- gamelist **does** re-read at least `napptype` / `nextid` tokens from that same string.
- Destination integer/string stores were **not** captured (no write_va); selection context beyond raw string re-read is unproven.
- This is **not** Class C as the primary lock — the list file never opens first.

`gwyblink` semantics this round: present as a suffix token in the entry string and read by cfunction; **no** evidence yet that it auto-selects or bypasses UI inside gamelist, because selection code never runs without a list.

## Selection / timer

- Selection-site CSV empty: cfg loader / select offsets never reached.
- Timer role after initial I/O: idle / UI refresh (no cfg poll, no network, no selected-state change).
- G6b (dynamic `lib.startGame`) remains deferred — correctly not pursued.

## Classification

### A: Completely no real cfg file or list read

First failed contract:

```text
CFG_FILE_OPENED
natural producer = gamelist.ext cfg loader (base+0x7B6C → plat 0x10112)
  preferred path: bare "cfg.bin" (MRP_MEMBER, 6898 — cannot hold index 36)
  then path SM → external "gwy/cfg.bin" (SHARED_ROOT, 20728 — holds cfg36 offline)
```

Observed stop: after gamelist entry + param token reads + natural timers, **before** any 10112/cfg open.

## Acceptance answers

```text
无 fast assist 是否仍进入 gamelist：YES
无 fast assist 是否仍到达相同 cfg 停点：YES（fmt mapped；未 open/select）

真实 cfg 数据源：host gwy/cfg.bin (20728) 存在；Guest 未加载。内部 gamelist member cfg.bin (6898) 不足以含 index 36
真实打开路径：NONE（Guest）
cfg 列表记录数：0（Guest）；host external ≈ (20728-1024)/272 = 72
是否存在完整 cfg36 record：Guest=NO；host offline=YES @ file_off=10816
cfg36 Guest 地址：N/A
cfg36 source offset：10816（host only）

启动参数是否被 cfunction 解析：YES（逐字段 token read @ 0x9B8C8）
解析结果写入地址：UNOBSERVED（write_va empty）
gamelist 是否读取解析结果：PARTIAL（napptype/nextid token re-read @ 0x2E77AE）
gwyblink 的真实语义：entry 后缀存在；本轮无自动选中证据（选择链未启动）

cfg36 选择谓词：N/A（无 record）
当前失败的第一项谓词：CFG_FILE_OPENED
负责满足该谓词的自然生产者：gamelist cfg loader → 0x10112（未执行）
是否等待真实用户输入：UNKNOWN / 尚未到达 UI 选择层

cfg36 是否由 Guest 自然选中：NO
selected state 写入 PC：N/A
post-select 第一个真实行为：N/A
是否出现 Guest startGame 调用：NO
当前唯一门锁：CFG_FILE_OPENED（gamelist 未进入真实 cfg 列表 I/O）
```

## Next (only)

```text
追清为何 gamelist 在读完 napptype/nextid 后不进入 cfg loader (base+0x7B6C / 0x10112)
不要伪造 open，不要写 selected index，不要调用 startGame
```

## Artifacts

- reports/p21_cfg36_selection_verdict.md
- reports/p21_cfg_file_io.csv
- reports/p21_cfg_record_inventory.csv
- reports/p21_launch_param_provenance.csv
- reports/p21_cfg_selection_branches.csv
- reports/p21_timer_state_diff.csv
- out/p21/p21_build_identity.txt
- research/runners/p21_run_cfg36_selection.ps1
- logs/p21_A_vmrp.txt / logs/p21_B_vmrp.txt
