# P20-CLEAN Shell Recovery Verdict

## Bottom line

**Class: B_cfg36 (gamelist reached; cfg36 not selected)**
**First fork vs historical E10A-3.1: restored through CONTINUE+isolation; now blocked at cfg36 selection (same historical ceiling+).**

NATURAL_ONLY held. No 0x10140 activator. No static capsule.

Historical map: user "old P20/P21" = repo E10A-3 / E10A-3.1b (no files named P20_*).

## What this round fixed

P16 nest guard previously blocked documented MRPGCMAP entry `base+8` (0x2EB814) inside UC_HOOK_CODE, producing WRONG_ENTRY_SELECTION and **no br_exit**.

P20-CLEAN deferred that entry and drained after MAP_FUNC leave:

```text
[JJFB_MRPGCMAP_ENTRY] result=DEFERRED reason=NESTED_EMU_IN_CODE_HOOK_BLOCKED
[JJFB_MRPGCMAP_ENTRY] op=DRAIN_PENDING count=1
[JJFB_MRPGCMAP_ENTRY] result=EMU_OK
→ br_exit_enter GWY_CONTINUE_READY
→ SHELL_CORE_CONTINUE → gwy/gamelist.mrp
→ GAMELIST_ERW_HOST_ISOLATED P=0x2AC8EC erw=0x682B8C
→ gamelist timer FIRE_EXT x3 (own helper 0x2E3099)
```

## Gate matrix (corrected)

| Gate | Pass | Note |
|------|------|------|
| G1 gbrwcore start | YES | |
| G2 0x10102 register | YES | handler observed 0x30B7F1 class |
| G3 natural timer/callback | YES | FIRE_EXT on gbrwcore then gamelist |
| G4 lazy/cfn | YES | CFN / entry path |
| G5 export table | YES | string table only |
| G6 startGame publish | PARTIAL | string_va_not_entry only; **no dynamic fnptr** |
| br_exit CONTINUE | YES | restored |
| enter gamelist | YES | |
| P/ERW isolation | YES | E10A31B HOST_ISOLATED |
| 0x30D5D2 fault | ABSENT | |
| cfg36 selected | **NO** | `SHELL_PHASE_CFG_FMT_MAPPED note=cfg36_param_fmt_not_selected` |
| G7 Guest startGame BL/BLX | **NO** | |
| nested jjfb | **NO** | |
| live capsule | **NO** (correct — Gate7 not hit) |

## Env diff (P19 → historical)

See `reports/p20_clean_env_diff.csv`. Critical restores: E10A31B isolation, E10A31 timer context, E10A shell trace, and **entry defer drain** (new). Kept FORCE_10140 off; excluded product E5 scheduler.

## Acceptance answers

```text
历史 Gates 1–6 是否在当前核心复现：部分 YES（G1–G5 + string G6；G6b 动态指针仍无）
第一个与历史 P20 分叉点：本轮已越过 P19 的 no-CONTINUE 分叉；当前门锁=cfg36 未选中（B）
gbrwcore callback 是否自然执行：YES（10102 注册 + 自然 FIRE_EXT）
API builder 是否自然执行：PARTIAL（export/CFN 有；无独立 API builder 标签）
startGame 指针是否动态发布：NO（仍 string_va_not_entry）

是否进入 gamelist：YES
gbrwcore / gamelist P 是否隔离：YES（同 P VA，独立 ERW 0x682B8C）
旧 0x30D5D2 fault 是否保持消失：YES（未见）
cfg36 是否自然选中：NO（fmt mapped / not selected）

是否捕获真实 Guest startGame 调用：NO
caller PC / branch / dynamic pointer / args / continuation：NOT_CAPTURED

是否进入 nested jjfb start_dsm：NO
是否生成 live research capsule：NO（禁止无现场造 capsule）
child init return 后第一个真实父级/平台行为：N/A
0x10140 首次激活者是否终于可裁决：NO
当前唯一门锁：cfg36 自然选择 / post-select → 真实 startGame 调用（裁决 B→C→D）
```

## Next (only)

```text
追真实 cfg 列表加载与 cfg36 选择合同
不要调用 startGame
不要实现 0x10140 activator
不要 sibling / 静态 capsule
```

## Artifacts

- reports/p20_clean_shell_recovery_verdict.md
- reports/p20_clean_gate_matrix.csv
- reports/p20_gamelist_startgame_trace.csv
- reports/p20_parent_child_live_frame.csv
- reports/p20_clean_env_diff.csv
- out/p20/p20_build_identity.txt
- logs/p20_clean_vmrp.txt
- research/runners/p20_run_clean_shell_recovery.ps1
- code: ext_mrpgcmap_entry_order defer+drain; gwy_ext_obs host_callback_leave drain
