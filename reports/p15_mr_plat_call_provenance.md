# P15 — `mr_plat` call provenance gate

## Verdict

**B_FALLTHROUGH_OR_STALE** — `mr_plat` 不是真实 Guest 调用；禁止接线 `br_mr_plat`。

Frozen: `direct_lr`, `product_ffp_apply_abi=0`, `current_mrp=gwy/jjfb.mrp`, owner=`robotol.ext`.  
本 gate **只裁决、不实现** `mr_plat`；P13/P14 通用平台实现可保留，但不得再声称它们代表 JJFB 主流程推进。

## Chain (hit1)

Case-9 / family deliver 后 `lr_after=0x304599`，随后 **mr_table 线性跌落**：

| seq | slot | api | class | R0 | R1 | LR |
|-----|------|-----|-------|----|----|-----|
| 12 | 0x28005C | DATA:reserve1 | **TABLE_DATA_EXECUTION** | 0 | 9 | 0x304599 |
| 13 | 0x280060 | DATA:_mr_c_internal_table | TABLE_DATA_EXECUTION | 0 | 9 | 0x304599 |
| 14 | 0x280064 | DATA:_mr_c_port_table | TABLE_DATA_EXECUTION | 0 | 9 | 0x304599 |
| 15 | 0x280068 | _mr_c_function_new | LINEAR_SLOT_FALLTHROUGH | 0 | 9 | 0x304599 |
| 16–18 | +4… | mr_printf / mem_get / mem_free | LINEAR_SLOT_FALLTHROUGH | stale | 9 | 0x304599 |
| 19 | 0x280078 | mr_drawBitmap | LINEAR_SLOT_FALLTHROUGH | 0 | 9 | 0x304599 |
| 20 | 0x28007C | mr_getCharBitmap | LINEAR_SLOT_FALLTHROUGH | 0 | 9 | 0x304599 |
| 21–24 | … | timer\* / getTime / getDatetime | LINEAR_SLOT_FALLTHROUGH | stale | 9 | 0x304599 |
| 25 | 0x280090 | mr_getUserInfo | LINEAR_SLOT_FALLTHROUGH | 0 | 9 | 0x304599 |
| 26 | 0x280094 | mr_sleep | LINEAR_SLOT_FALLTHROUGH | −1 (prev ret) | 9 | 0x304599 |
| 27 | 0x280098 | **mr_plat** | **LINEAR_SLOT_FALLTHROUGH** | 0 | 9 | 0x304599 |

每个 `slot_delta=+4`，`same_lr=1`，入口前 **无** BL/BLX 指向当前 stub（`branch=-`）。  
`mr_sleep` 的 `R0=0xFFFFFFFF` 即上一跳 `getUserInfo(NULL)→MR_FAILED`；`mr_plat` 的 `(code,param)=(0,9)` 是 Case-9 残留，不是 Guest 生产的 plat code。

MAP_DATA stub（`reserve1` 等）在 `hook_code` 里 **不写 PC=LR**，unicorn 会继续执行表内数据字 → 真正的表遍历；随后 MAP_FUNC 在 `PC=LR` 后仍以同一 LR / 未刷新参数连撞下一槽。

## Acceptance answers

```
mr_getCharBitmap 是否真实 Guest 调用：否 — LINEAR_SLOT_FALLTHROUGH
mr_getTime 是否真实 Guest 调用：否 — LINEAR_SLOT_FALLTHROUGH
mr_getUserInfo 是否真实 Guest 调用：否 — LINEAR_SLOT_FALLTHROUGH
mr_sleep 是否真实 Guest 调用：否 — LINEAR_SLOT_FALLTHROUGH（R0=上一 API 返回值）
mr_plat 是否真实 Guest 调用：否 — LINEAR_SLOT_FALLTHROUGH

mr_plat caller PC：无（无 Guest branch）
mr_plat branch instruction：无
mr_plat code：0x0（无效 — Case-9 / 表走残留）
mr_plat param：0x9（无效 — app=9 残留）
R0/R1 最后写入位置：进入窗口前未见 Guest 为 plat 赋值；R0 链来自 host 返回值传递
mr_plat LR continuation：0x304599（贯穿整串，非 plat caller continuation）

是否发生 bridge slot 顺序跌落：是
第一个错误入口：DATA:reserve1 @ 0x28005C（TABLE_DATA_EXECUTION）
stale LR 来源：PLATFORM_FAMILY_EVENT DELIVER_DONE lr_after=0x304599；嵌套 restore 亦见 lr_polluted=1（inner_lr 曾为 0x304599）
是否修复控制流：否（本 gate 只裁决）
修复后第一个真实自然行为：N/A — 待修最早 continuation / MAP_DATA 跌落

原冒泡环境是否观察到同一调用：未在本轮对照（产品路径已足以定案 B）
是否允许实现当前 mr_plat code：否 — 禁止接线 br_mr_plat
是否出现真实游戏画面：否
```

## Rule (post-verdict)

- **禁止** `br_mr_plat` / platEx / ferrno mega-switch。
- 修复必须来自已保存的 outer PC/LR（或阻止 MAP_DATA 当代码执行），不得硬编码地址。
- P13/P14 的 `mr_getCharBitmap` / `mr_getTime` / `mr_getUserInfo` / `mr_sleep` **实现可保留**为通用能力；`mr_sleep` 的 `ms>10000→0` 仅作诊断钳制，**不得**计为推进成功。
- 修好控制流后重新自然跑，报告真正的第一个平台调用。

## Nest audit note

`[JJFB_BRIDGE_NEST_INNER]` 多次 `lr_polluted=1`：inner_lr ≠ outer_lr（含 `inner_lr=0x304599`）。与 Case-9 后 LR 粘在 robotol `0x304599` 一致，指向嵌套 runCode / family deliver 的 continuation 污染，而不是 `mr_plat` 业务入口。

## Artifacts

- `out/p15/p15_build_identity.txt`
- `out/p15/bridge_entry_provenance.csv`
- `out/p15/bridge_predecessor_ring.csv`
- `out/p15/bridge_insn_ring.csv`
- `out/p15/bridge_nest_audit.csv`
- `reports/p15_bridge_call_matrix.csv`
- `logs/p15_hit1_vmrp.txt`
- Runner: `research/runners/p15_run_plat_provenance.ps1`
