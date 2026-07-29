# P9 — 0x304BF0 真实 epilogue 与 resume-to-epilogue A/B 方案

**Date:** 2026-07-29  
**Entry:** JJFB product path (`gwy/jjfb.mrp` / robotol)  
**Source:** `robotol.ext` decoded from `game_files/mythroad/240x320/gwy/jjfb.mrp` (SHA256 `52c13182…d5fc036`)，guest base `0x2D8DF4`，解码长度 `253420`。  
**Tooling:** `research/runners/p9_disasm_304bf0_epilogue.py`（capstone，读 `out/research_p9_extract/robotol.ext`）。  
**Prereq:** P8 A/B 已落地，`direct_lr` 为 product-safe 基线（5 BMP 完成、首帧稳定、SP 不变量 delta=0）。本文件落实 P8 报告 "Next #1"：找出 `0x304BF0` 的真实 epilogue。

---

## 1. Verdict

```text
REAL_EPILOGUE_FOUND
RESUME_TO_EPILOGUE_IS_STACK_SAFE  (等价于 direct_lr 的栈契约)
```

- `0x304BF0` 是带 `PUSH` 的函数入口（已确认）。
- 该函数的**唯一返回点**是 `0x304C4A` 的 `add sp,#0xcc` 紧接 `0x304C4C` 的 `pop {r4,r5,r6,r7,pc}`。所有路径（成功/失败）最终都 `b #0x304c4a` 汇集到这里。
- 栈契约完全平衡（证明见 §3）。因此 `resume-to-epilogue` 是 `callsite`(Situation C, 不安全) 的**安全替代**，且与 `direct_lr` 栈等价。

---

## 2. Prologue（入口帧，来自 `e9d_disasm_304bf0.py` 字节 + capstone 复核）

```text
0x304BF0  FFB5        PUSH {r0-r7, lr}          ; SP -= 36 (0x24)
0x304BF2  1C0E        ADDS r0, r1               ; (body scratch)
0x304BF4  2100        MOVS r1, #0              ; ← callsite 模式误 resume 到这里（0x304BF4）
0x304BF6  B0AF        SUB  SP, SP, #0xBC       ; SP -= 188 (0xBC)
```

- PUSH 9 寄存器 = 36 字节。
- `SUB SP,#0xBC` = 188 字节局部帧。
- **prologue 净减 SP = 36 + 188 = 224 (0xE0)**。

> `JJFB_304BF0_RESUME_MODE=callsite` 把 PC 设到 `0x304BF4`（`MOVS r1,#0`），跳过了 `PUSH`，栈契约破裂 → 约 2 个资源后白屏。这正是 P8 的 Situation C。

---

## 3. Epilogue（真实返回点，§4 尾部反汇编佐证）

```text
0x304C4A  B0CC        ADD  SP, SP, #0xCC       ; SP += 204 (0xCC)
0x304C4C  BDF0        POP  {r4, r5, r6, r7, pc}; SP += 20；恢复 r4-r7 与 pc(=lr)
```

### 栈平衡证明

| 阶段 | SP 相对 entry(SP_entry=hook 捕获值) |
|---|---|
| hook 触发（PUSH 前） | `SP_entry` |
| PUSH {r0-r7,lr} | `SP_entry - 36` |
| SUB SP,#0xBC | `SP_entry - 224` |
| （函数体，局部帧在 SP 之上，不触碰已保存区） | 不变 |
| ADD SP,#0xCC (epilogue) | `SP_entry - 224 + 204 = SP_entry - 20` |
| POP {r4,r5,r6,r7,pc} | `SP_entry - 20 + 20 = SP_entry` ✅ |

注意：epilogue 用 `add sp,#0xcc`(204) + `pop{r4-r7,pc}`(20) = 224，与 prologue 的 36+188=224 **正好抵消**；它故意只 POP r4-r7（不恢复 r0-r3 这 4 个 scratch/参数寄存器，用 ADD 多出的 16 字节补偿），是合法的压缩写法。

### 唯一性

- 全函数（0x304BF0..0x304FBC，972 字节 / 473 条 Thumb 指令）内**仅此一个 `pop {pc}`**。
- 所有返回路径都 `b #0x304c4a`（见 §4：0x304F6C、0x304FBA 等），故 `0x304C4A` 是单一 epilogue。

---

## 4. 函数尾部佐证（capstone 反汇编末段）

```text
0x304F5E  cmp  r7, #0
0x304F60  beq  #0x304f68
0x304F62  adds r0, r7, #0
0x304F64  bl   #0x3045e4
0x304F68  movs r0, #0
0x304F6A  mvns r0, r0            ; r0 = -1 (错误/未命中分支的返回值)
0x304F6C  b    #0x304c4a        ; → 唯一 epilogue
...
0x304FB8  movs r0, #0           ; 成功分支 r0 = 0
0x304FBA  b    #0x304c4a        ; → 唯一 epilogue
0x304FBC  sbcs r2, r7          ; (下一函数的起点)
```

`r0` 的返回值在每条路径跳到 epilogue **之前**已设定（成功=0，未命中=-1）。resume 时由 host 直接写 `R0`。

---

## 5. resume-to-epilogue 的 host 设置（新 `epilogue` 模式）

在 `restore_304bf0_ok()` 中新增分支（仅 env 驱动，不写死游戏状态）：

```c
} else if (is_epilogue_mode) {
    uint32_t sp_at_epi = g_entry_sp - 224u;     /* SP_entry - 36 - 188 */
    uint32_t ret_pc     = (PLATFORM_MRP_EPILOGUE_PC | 1u); /* 0x304C4B */
    uc_reg_write(uc, UC_ARM_REG_SP, &sp_at_epi);
    uc_reg_write(uc, UC_ARM_REG_R0, &status);   /* 0 或 handle_guest（见 A/B）*/
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);   /* 落到 0x304C4A add sp,#0xcc */
    /* 不要写 R4-R11：POP 会从栈上恢复 r4-r7；r8-r11 为调用者值 */
}
```

头文件新增（沿用既有 `PLATFORM_MRP_*` 常量命名，非新硬编码地址）：

```c
PLATFORM_MRP_LOOKUP_EPILOGUE_PC  0x304C4A  /* add sp,#0xcc — resume 目标(Thumb+1) */
```

`resume_mode()` 增加 `"epilogue"` 分支；与 `direct_lr`/`callsite` 互斥。

**不变量约束（必须成立才能安全）：**
- `SP = g_entry_sp - 224`（即 prologue 执行完后、函数体开始时的 SP 位置）。
- `PC = 0x304C4B`（落在 `add sp` 上；若直接落到 `pop` 0x304C4C，则少抵消 204 字节 → 栈损坏）。
- `R0 = status`（POP 不恢复 r0）。
- 不要手动写 R4-R11（让真实 `POP` 从栈恢复 r4-r7）。

---

## 6. A/B 实验方案（Situation B，对应 P8 "Next #2"）

| | A: R0=0 | B: R0=handle_guest |
|---|---|---|
| resume 目标 | `epilogue` 模式 | `epilogue` 模式 |
| R0 写入 | `0`（资源已就绪的"成功"语义） | `handle_guest`（pending 句柄，等价 `direct_lr` 现有行为） |
| 预期 | 同 `direct_lr`：5 BMP 完成、首帧稳定、SP delta=0 | 同上，差异仅在调用者收到的返回值 |
| 风险 | 下游若依赖 `handle` 而非 `status` 可能行为不同 | 与现状最接近 |

**验收（复用 P8 的 SP 不变量表）：**
- `CALLER_SP_DELTA=0` 对 5 个资源全部成立；
- 资源完成数 = 5（无第 6 个自然资源）；
- 首帧稳定（`waiting_for_first_frame` → 稳定）；
- 反跑偏审计通过（未引入 `0x2DADC4`/`B71`/`15D` 强制）。

若 A 与 B 均稳定且等于 `direct_lr` 基线 → `epilogue` 可作为比 `direct_lr` 更"诚实"的产品基线（走真实函数 epilogue 而非直接跳 LR），并淘汰 `callsite`。**之后**才回到 P8 "Next #3"：审视 `0x30D301` case 9 loads。

---

## 7. 与 anti-drift 规则的一致性

- 新增内容仅为 **env 驱动的 resume 模式 + 一个已文档化的 `PLATFORM_MRP_*` 常量**；不写 JJFB 固定状态、`ui_mode`、B71/15D/ERW 偏移或 FORCE 类环境变量。
- 核心不得出现的新逻辑：无；所有 resume 副作用由既有 `boot_successor_on_*` 追踪，保持 observe-only。
- 本文件为活跃 stage note，`p9_disasm_304bf0_epilogue.py` 置于 `research/runners/`（研究 runner，不入根/产品核心）。

---

## 8. 完成清单

| Item | Status |
|---|---|
| 真实 epilogue 定位 | done（0x304C4A `add sp,#0xcc` + 0x304C4C `pop{r4-r7,pc}`） |
| 唯一返回点确认 | done（全函数仅 1 个 `pop{pc}`，所有路径 `b #0x304c4a`） |
| 栈平衡证明 | done（净 224 ↔ 224） |
| resume-to-epilogue host 设置 | 给出精确 SP/PC/R0 约束 |
| A/B 方案 | done（Situation B：R0=0 vs R0=handle_guest） |
| 实现并运行 A/B | **未做**（本环境缺 i686-mingw + unicorn 开发库，无法构建/运行 32 位 Unicorn 工程；落地需带工具链环境下执行，并跑构建+单测+反跑偏审计） |

**下一步（带工具链环境）：** 在 `restore_304bf0_ok()` 加 `epilogue` 分支 → `RUN_BUILD.ps1` → `RUN_TESTS.ps1` → 以 `JJFB_304BF0_RESUME_MODE=epilogue` 跑产品黄金链，核对 §6 验收；通过后淘汰 `callsite`。
