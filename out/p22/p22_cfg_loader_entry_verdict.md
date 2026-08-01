# P22-CLEAN cfg loader entry predicate verdict

## Bottom line

**Class: F — cfg loader caller itself not scheduled**

Lane A freeze held. `JJFB_P22_MODE` / `JJFB_P22_HEADLESS_SELECT` stayed OFF. No Guest cfg open, no forge.

The first closed lock is not “cfg.bin missing” and not a taken branch inside `+0xFF00`. It is:

**`+0x10740` (UI/init that owns `bl +0x10814 → +0xFF00 → +0x7B6C`) never executes.**

## P21 freeze checks

| Check | Result |
|------|--------|
| gbrwcore entry | PASS |
| br_exit CONTINUE | PASS |
| gamelist entered | PASS |
| gamelist ERW isolated | PASS (`erw=0x682B8C`) |
| natural FIRE_EXT | PASS |
| forced 10140 | PASS (=0) |
| 0x30D5D2 fault | PASS (=0) |
| old P22 headless OFF | PASS |

## Runtime module identity

```
gamelist runtime base: 0x2D4364   (after RAW_BASE_REFINE pad=0x4)
gl_size: 0x165DC
sha256: e510fe8795381f27e1ec49048f04ee94a486baf0ede0c9382825d2d606427ca8
bytes_ok +0x77AE: 1
bytes_ok +0x7B6C: 1   (push {r4-r7,lr} = function ENTRY)
gl_insn observed: 5460
```

## Correcting P21’s “+0x77AE” label

P21 logged abs PC `0x2E77AE` as `base+0x77AE` when a prior run’s base was `0x2E0000`.

This Lane A base is `0x2D4364`, so:

| | abs PC | module_offset | real code |
|--|--------|---------------|-----------|
| P21 label | 0x2E77AE | *assumed* 0x77AE | — |
| P22 measure | 0x2E77AE | **0x1344A** | `ldrh r1,[r4]` inside `+0x133E0` — **0x10204 tagged-buffer check** (`cmp #1/#3`), reads `0x0001` |

Hits: `+0x133E0=3`, `+0x1344A=3`, historical `+0x77AE=0`.

So: gamelist does **not** “consume napptype/nextid at +0x77AE” on this load map. The mem-read into the launch-param VA range was an alias/overlap with a platform tag word, not descriptor field parsing. cfunction still token-reads the entry string (`parser_cfn_enter=1`).

## cfg loader

```
+0x7B6C = CFG LOADER function ENTRY
direct BL callers:
  +0xD96C  (wrap +0xD964)
  +0xFF3A  (inside +0xFF00)
  +0xFFB2  (inside +0xFF00)
classification this run: all CALLER_NOT_REACHED
nearest scheduled layer missing: +0x10740 (hit=0)
```

## First blocking fact (PASS chain)

```
napptype/nextid string read (cfunction) ✓
→ gamelist +0x133E0 timer/UI helper (0x10204 tag path) ✓
→ +0x10740 UI/init that leads to cfg loader ✗ NEVER ENTERED
→ +0x10814 bl +0xFF00 ✗
→ +0x7B6C cfg loader ✗
→ plat 0x10112 ✗
```

| Field | Value |
|-------|-------|
| branch_pc / block | N/A (function not reached) |
| block_off | **+0x10740** |
| branch_instruction | `CALLER_NOT_REACHED +0x10740` |
| actual path | `+0x10740` never executed |
| cfg-loader path | `+0x10740 → +0x10814 → +0xFF00 → +0x7B6C` |
| predicate field | n/a until `+0x10740` is entered |
| next candidate fields *inside* 10740 | `[R9+0x3E4]` once-flag; `[R9+0x6C4]` mode==0xF |
| natural producer | callers of `+0x10740` (static: `+0x4076`, `+0x12D0E`, …) — event/UI dispatch |
| 0x10800 ack affects this gate? | **No evidence** (`ack_seen=0` in window; gate is scheduling) |
| waiting timer/event/UI? | **Yes — Class F / event path** (producer of `+0x10740` call) |
| real cfg open? | NO |
| Guest state forged? | NO |

## Parameter descriptor contract

| Question | Answer |
|----------|--------|
| gamelist parse raw string vs cfunction struct? | cfunction reads raw entry string; gamelist “0x2E77AE” hit is **not** string parse |
| return = success vs token found? | Unobserved at gamelist descriptor layer this round |
| unread fields invalidate descriptor? | **Not the current lock** — cfg path never gets that far |

## Classification

### F: cfg loader caller not scheduled

Next minimal work (not this round’s forge list):

1. Dynamic-hit callers of `+0x10740` (`+0x4076`, `+0x12D0E`, …).
2. Close *their* entry predicate (which event/timer/platform service should call them).
3. Only after `+0x10740` runs, evaluate `R9+0x3E4` / mode `0xF` / `+0xFF00`/`+0x7DB0` gates.

Do **not** Host-call `+0x7B6C`, do **not** open cfg for Guest, do **not** re-enable headless select.

## PASS answers

```
gamelist runtime base: 0x2D4364
+0x77AE 的真实函数/作用: packed-field reader mid-insn (bytes present); 本轮未执行。P21 同名 abs PC 实际是 +0x1344A
+0x7B6C 的真实函数/作用: cfg loader 函数入口 (push {r4-r7,lr}) → 通向 plat 0x10112

cfg loader 的全部真实调用者: +0xD96C, +0xFF3A, +0xFFB2
最接近执行的 caller: 上层 +0x10814 (via +0x10740) — 均未到
caller 是否被执行: NO (hit7B6C=0 hitFF00=0 hit10740=0)

阻止 cfg loader 的第一条分支: CALLER_NOT_REACHED of +0x10740
branch PC: N/A (function not entered)
比较操作数: N/A
实际分支: +0x10740 never scheduled
通向 loader 的目标分支: enter +0x10740 then +0x10814→FF00→7B6C

阻断谓词字段地址: N/A (scheduler layer)
ERW/P 相对偏移: (next layer candidates R9+0x3E4 once-flag, R9+0x6C4 mode)
当前值 / 期望值: N/A
最后写入者 / 自然生产者: callers of +0x10740 (event/UI path) — 本轮从未观察到

参数 descriptor 是否完整: 未在 gamelist 侧证明；非当前门锁
0x10800 ack 是否影响该 gate: NO evidence this round
是否等待 timer/event/UI: YES — wait for natural call into +0x10740

是否出现真实 cfg open: NO
是否修改任何 Guest 状态: NO
当前唯一门锁: Class F — +0x10740 not scheduled
下一处最小通用修复: 恢复会自然调用 +0x10740 的平台/事件调度合同（不上 cfg forge）
```

## Artifacts

- `reports/p22_cfg_loader_entry_verdict.md`
- `reports/p22_cfg_loader_xrefs.csv`
- `reports/p22_param_to_cfg_dynamic_slice.csv`
- `reports/p22_cfg_entry_predicate_provenance.csv`
- `reports/p22_param_to_cfg_branch_chain.md`
- `reports/p22_gamelist_cfg_path_disasm.txt`
- `out/p22/p22_build_identity.txt`
- `out/p22/p22_runtime_summary.txt`
- `out/p22/gamelist_cfg_path_runtime.bin` / `.sha256`
- `research/runners/p22_run_cfg_loader_predicate.ps1`
