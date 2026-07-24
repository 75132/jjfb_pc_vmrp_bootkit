# Task 8: Field Parser Exit Predicate + Nested Scheduling Closure

**Date:** 2026-07-25  
**Run:** `fp8_B_20260725_013726` (Variant B, 75 s)  
**Baseline:** Task 7 `h2_B_20260725_012349`  
**Primary verdict:** `FIELD_PARSER_EXIT_PREDICATE_IDENTIFIED` + `FIELD_LENGTH_INVALID`  
**Scheduling verdict:** `NESTED_EVENT_NOT_REQUIRED_FOR_LOOP` (not scheduling deadlock)

---

## Core answer

> **H1（字段解析合同错误）成立；H2（嵌套调度等待）不成立。**

内环 `0x30A100..0x30A110` 的退出谓词已精确解码：**`CMP r1, r5` @ `0x30A10E`，`BLT` 回边 @ `0x30A110`**，当 **`r1 >= r5`（无符号）** 时从 `0x30A112` 正常退出。当前 **`r5=0x7374`** 被设为字段字节数上界，导致需约 **0x7374（29556）** 次迭代；在观测窗口内 **`r1` 单调递增**（iter=40000 时 `r1=0x4591`），但 **`r5` 永远不满足**，表现为“稳定 PC 环 + 白屏”。**`r5=0x7374` 动态来源于 stream @ offset `0x2F9CC`，对应 `"stat"` 字段名的 `'st'` 半字，不是 queue/nested 状态。**

| 问题 | 答案 |
|---|---|
| 内环退出谓词？ | **`r1 >= r5`** @ `0x30A10E/0x30A110` |
| 当前 lhs/rhs？ | **`r1` 递增（iter40000→0x4591）；`r5=0x7374` 冻结** |
| `R5=0x7374` 来源？ | **`0x30A0EA` 写 r5；stream_off=`0x2F9CC`；ASCII `'st'`** |
| stream cursor 合法？ | **cursor=`0x2B23B4`（heap 拷贝），remain=`0x10`；inner=`0x2829E8`** |
| nested 后 consumer 再入？ | **是** — `0x2DC80C` @ q=2，helper=1（seq 12–14） |
| 循环依赖 nested 完成？ | **否** — 内环仅 `[r4]`/`[r6+r2]` stream 字节访问 |
| 第一处错误合同？ | **`0x30A0CC` 前置路径将 stream 半字 `'st'` 载入 r5 作为字段长度** |

---

## 一、内环指令解码（`0x30A0E0..0x30A130`）

静态反汇编：`reports/field_parser_loop_disasm.md`

### 关键块

| PC | raw | mnemonic | 作用 |
|---|---|---|---|
| `0x30A0F6` | `CF F7 59 FC` | **BL `0x2D99AC`** | strdup / malloc 字段 |
| `0x30A0FE` | `08 DD` | **BLE → `0x30A112`** | r5==0 快速退出 |
| **`0x30A100`** | — | **循环头** | |
| `0x30A100` | `22 68` | LDR r2,[r4,#0] | 读 state 游标 |
| `0x30A102` | `B2 5C` | LDRB r2,[r6,r2] | 读 stream 字节 |
| `0x30A104` | `42 54` | STRB r1,[r0,r2] | 写输出缓冲 |
| `0x30A108` | `01 31` | ADDS r1,#1 | **携带变量：索引++** |
| `0x30A10E` | `A9 42` | **CMP r5,r1** | **退出谓词** |
| **`0x30A110`** | `F6 DB` | **BLT → `0x30A100`** | **回边** |
| **`0x30A112`** | `00 21` | MOVS r1,#0 | **正常退出** |

```text
loop_head 0x30A100:
    load/store stream byte via [r4]/[r6]
    r1++
    if (r1 < r5) goto loop_head   // BLT @ 0x30A110
exit 0x30A112:
    return from field parse
```

内环 **不读取** queue count、nested flag、ER_RW+B54/B60。

---

## 二、动态 trace（`JJFB_FIELD_PARSER_TRACE=1`）

### 循环携带变量

| 寄存器 | 行为 | 结论 |
|---|---|---|
| **r1** | iter1→0, iter2→1, iter3→2, iter10000→0x270F, iter40000→0x4591 | **每轮递增（非 spin）** |
| **r5** | 全程 `0x7374` | **上界冻结 — 根因** |
| **r4** | `0x2B23B4`（parser state） | 不变 |
| PC | `0x30A100..0x30A110` 8-block 环 | 与 Task 7 一致 |

**不是** zero-progress poll loop；是 **length 上界过大导致的极慢字节拷贝环**。

### R5 写入链

```text
[FP_R5] pc=0x30A0EA  old=0x0    new=0x7374  stream_off=0x2F9CC
[FP_R5] pc=0x30A0FA  old=0x7379 new=0x7374  stream_off=0x2F9CC
```

- **`0x7374` = ASCII `'st'`**（低 16 位）
- PAH inner bytes：`...73746174...` = `"stat"` + BE framing
- **动态证明**：非巧合 — writer PC + stream offset 一致

---

## 三、Stream 边界 @ `0x30A0CC` 入口

```text
[FP_ENTRY] base=0x2829E8 cursor=0x2B23B4 end=0x2829F8 remain=0x10 b60=0x4 r5=0x0
[PAH_INNER] 00000000 73746174 10000000 0C000000 05000000 ...
            ^zeros    ^"stat"   ^BE hdr   ^field len=0x10?
```

| 项 | 值 | 说明 |
|---|---|---|
| inner stream | `0x2829E8` | Path-A entry +4 |
| 工作 cursor | `0x2B23B4` | heap 拷贝区（非 inner 直接指针） |
| remain | `0x10` | 仅 16 字节 — **远小于 r5=0x7374** |
| B60 | `0x4` | record/field 计数（本 run 入口时） |
| BE(-1) 终止符 | **未到达** | cursor 未推进到 record 尾 |

**结论：** `r5=0x7374` 作为字段长度 **超过 remaining=0x10`** → **`FIELD_LENGTH_INVALID`** + **`FIELD_CURSOR_MISALIGNED`**（cursor 在 heap 镜像而非 framed stream 游标）。

---

## 四、Nested 调度时间线（`0x312A60` 之后）

| seq | event | q | helper | cb | cons | 说明 |
|---|---|---|---|---|---|---|
| 1–3 | drain_sched/trigger | 1 | 0 | 0 | 0 | helper 进入前 |
| 4–6 | consumer_enter/exit | 1 | 0 | 0 | 1→0 | **首次 consumer** |
| 7 | drain_delivered | 1 | 1 | 2 | 1 | helper 活跃中 |
| 8 | **nested_publish** | 1 | 1 | 2 | 1 | code=5 push |
| 9–11 | drain_sched/trigger | 2 | 1 | 2 | 1 | q→2 |
| **12–13** | **consumer_enter** | **2** | **1** | **2** | **2–3** | **nested 后再入 ✅** |
| 14 | consumer_exit | 2 | 1 | 2 | 2 | pop/dispatch |
| — | PAH_REENTER | — | 1 | — | — | nested `0x2E4040` 重入 |

**回答 Task 8 必答题：**

- nested 发布后 **`0x2DC80C` 再次进入？** → **是**（q=2，helper active）
- 是否 pop nested？ → consumer 运行并 dispatch（`PAH_REENTER`），但 helper **仍卡在内环**
- 循环是否等待 nested？ → **否**

→ **`NESTED_EVENT_NOT_REQUIRED_FOR_LOOP`**  
→ **排除 `NESTED_EVENT_SCHEDULING_DEADLOCK`**

---

## 五、假设裁决

| 假设 | 裁决 | 证据 |
|---|---|---|
| **H1** 字段解析/长度/游标错位 | **✅ 成立** | r5='st' 半字；remain=0x10 << r5 |
| **H2** 嵌套事件调度等待 | **❌ 不成立** | consumer 再入；内环无 queue 读 |

---

## 六、结果分类（证据支持）

```text
FIELD_PARSER_EXIT_PREDICATE_IDENTIFIED   ✅
FIELD_LENGTH_INVALID                     ✅  (r5=0x7374 vs remain=0x10)
FIELD_CURSOR_MISALIGNED                  ✅  (cursor=heap copy)
NESTED_EVENT_NOT_REQUIRED_FOR_LOOP       ✅
NESTED_EVENT_SCHEDULING_DEADLOCK         ❌  (证伪)
STRDUP_LENGTH_OR_TERMINATION_BROKEN      ⚠️  (待下一域：0x30A0F6 arg0=0x7375)
```

---

## 七、下一修复域（定位完成，本轮未修复）

**允许下一轮的修复方向（stream/parser 合同）：**

```text
0x30A0CC 入口：正确 BE field length → r5
stream cursor 推进合同（heap copy vs inner base）
0x10132 strdup 长度与 BE framing 对齐
```

**禁止：** 强改 r5、跳过 `0x30A100`、手动 pop queue、伪造 nested completed。

**完成型目标仍未达成：**

```text
0x30A0CC return → 0x2F68E4 → 0x2E4066 → 0x2DADC4
```

Logs: `out/helper_2f68e4_task8/B/`  
Disasm: `reports/field_parser_loop_disasm.md`  
Env: `JJFB_FIELD_PARSER_TRACE=1` + `JJFB_HELPER_2F68E4_TRACE=1`
