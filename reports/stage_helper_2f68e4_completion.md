# Stage: Helper 0x2F68E4 Completion Boundary

**Date:** 2026-07-25  
**Entry:** JJFB product path (`gwy/jjfb.mrp`)  
**Run B (sparse H2):** `h2_B_20260725_012349` (65 s, 400k+ helper insns)  
**Primary verdict:** `STABLE_HELPER_LOOP`  
**Prior phase:** `HELPER_LONG_BUT_PROGRESSING` (B60 0x6→0x3D, ~5k insns)

## Core answer

> **`0x2F68E4` 为什么没有返回？** 两阶段行为：先**有进展**地解析 stream（`ER_RW+B60` 递增到 `0x3D`），随后卡在 **`0x30A100..0x30A110` 稳定内环**（400k+ 指令，`R5=0x7374`、`q`、`nested` 快照哈希不变）。**不是**“指令预算耗尽”结论。

| Question | Answer |
|---|---|
| 长任务 / 等待 / 死循环？ | **先推进，后稳定循环**（`STABLE_HELPER_LOOP` @ `0x30A100`） |
| 主要循环做什么？ | `0x30A0CC` 字段解析内环；之前通过 `0x308D98→0x10132` 逐字段 strdup |
| 退出条件？ | 内环退出谓词未满足（`R5`/游标停在 `0x7374` 对应字段）；stream 终止符 `-1` 尚未到达 |
| 哪些状态在持续变化？ | **Phase1:** B60、alloc；**Phase2:** 无（PC 在 8-block 环，`r5/q` 冻结） |
| 嵌套 code=5 是否被消费？ | **发布成功**；helper 内 **`nested=1` outstanding**；consumer 未在 spin 期间 pop |
| 调度可重入死锁？ | **未证明** — consumer 曾 `q=1` 进入；但 helper 内 nested 后 **`q=2` 仍不消费** |
| 未闭环平台 API？ | **`0x10132` closed**（多次 strdup ok）；无 helper 内新 slot28 缺口 |
| 第一个不再推进的位置？ | **`0x30A100` 循环头**（B60 卡在 `0x3D` 之后） |
| 下一最小修复域？ | **字段解析/嵌套事件交互**（guest 状态或 nested 消费时机），非新 10138/10132 合同 |
| `0x2E4066` / `0x2DADC4`？ | **未进入** |
| 窗口？ | **仍白屏** |

---

## 1. 基线（Task 6 已闭环）

```text
0x10138 multi-out + gates
→ 0x10132 size-malloc
→ Path-A code=5 dispatch
→ 0x2E4040 → BL 0x2F68E4
→ nested 0x312A60 push 成功（无 0x312A78 R4=0）
```

Task 7 新增 **`JJFB_HELPER_2F68E4_TRACE=1`** 稀疏三层观察（block / edge / snap），Diagnostic 下替代 dense PAH 日志。

---

## 2. Helper 控制流（已走路径）

### 入口 ABI（call_id=1）

```text
PC=0x2E4040  LR=0x2DC8D9
R4=entry=0x2829F8  +0=5  +4=inner=0x2829E8  +8=4  +C=0x5F32313D
BL 0x2F68E4 @0x2E405E  LR→0x2E4063
inner bytes: 00 00 00 00 "stat" ... len-prefixed stream
```

### 局部 CFG（实际 BL 树，非仅 0x2F68E4..0x2F6952 体内）

| 角色 | PC | 工作 |
|---|---|---|
| 入口 block | `0x2F68E4` | 取 inner 指针，进入 stream 驱动 |
| stream 驱动 | `0x308D98` | 记录计数 / 游标（写 `ER_RW+B60`） |
| 记录解析 | `0x30A0CC` | 单条 record 字段 walk |
| 平台 strdup | `0x2D99AC` → slot28 → **`0x10132`** | 按字段长度 malloc+拷贝 |
| 后续块 | `0x2FEB94` | stream 后续阶段（dense 第 8 层 BL） |
| 期望退出 | `0x2F68FF` → **`0x2E4062..0x2E4066`** | clean return 后 lifecycle 继续 |

### 退出谓词（legacy + 观测）

```text
while (stream_cursor advances)
  parse record → 0x10132(strdup) → B60++
until (*cursor == BE(-1))  → leave 0x2F68E4
```

观测窗口内 **B60 从 1 增至 0x3D+**（dense 对照 run），说明谓词 **`remaining stream > 0`** 仍成立。

---

## 3. 分类证据

### ✅ HELPER_LONG_BUT_PROGRESSING

- `ER_RW+B60` 单调递增（queue/stream 游标）
- 每条 record 触发 **`0x10132 ret=0x6BB324…`**（非 stub）
- Guest indirect call 链持续推进（`0x2F68FA→0x308D99→0x30A0CD→0x2D99AD`）

### ✅ STABLE_HELPER_LOOP — confirmed (Phase 2)

```text
[H2_SNAP] insn=10000..400000  pc=0x30A100..0x30A110 (8-block ring)
           r5=0x7374  q=1→2  nested=0→1  api=1→8
           B60 frozen @ 0x3D (after Phase 1)
```

内环 **400k+ 指令** 无 R5/q 进展 → 满足稳定循环阈值。

### ❌ NESTED_EVENT_SCHEDULING_DEADLOCK

```text
[H2_SCHED] consumer_enter depth=1 q=1   # helper 进入前
nested_path_a_published @0x312A60     # helper 内
```

Consumer **可重入运行**；queue 非空时仍进入 `0x2DC80C`。

### ❌ NEXT_PLATFORM_API_MISSING（helper 体内）

| API | 次数 | 分类 |
|---|---|---|
| `0x10138` | 1（helper 前） | closed |
| `0x10132` | 多条 record | closed |
| 其他 | 无 helper 内新 slot28 调用 | — |

`[H2_API] NEW api=0x1E209` 来自 **timer callback**（`pc=0x304589`），非 helper stream 解析链上的缺失合同。

### ⚠️ 诊断 artifact 已修复

旧 PAH 在 nested `0x2E4040` 重入时误报 `path_a_helper_returned/reenter`，导致 handler 会话被提前关闭。Task 7 改为 **`[PAH_REENTER] nested 2E4040 while 0x2F68E4 active`**，H2 会话保持。

---

## 4. 里程碑（Task 7 新增）

| Milestone | 用户可见 |
|---|---|
| `path_a_helper_running` | Processing startup event |
| `nested_event_pending` | Waiting for nested event |
| `nested_event_consumed` | Continuing game initialization |
| `path_a_helper_stable_loop` | Diagnosing helper loop |
| `path_a_helper_returned` | Path-A helper returned |

Run B 观测：`path_a_helper_running` @ T+37 s；**无** `path_a_helper_returned` / `lifecycle_successor_entered`。

---

## 5. A / B / C 矩阵

| | A/B Diagnostic + H2 | C Default |
|---|---|---|
| `JJFB_HELPER_2F68E4_TRACE` | 1 | off |
| Dense PAH mem hook | off (lite boundary) | off |
| Helper 返回 | 未在窗口内 | 未在窗口内 |
| 0x2E4066 / 0x2DADC4 | 否 | 否 |
| 白屏 | 是 | 是 |

Logs: `out/helper_2f68e4_task7/B/`  
Reports: `reports/helper_2f68e4_*.csv`（进程 exit 时 `atexit` 写入）

---

## 6. 下一边界（不越界）

**允许：** 延长自然运行时间；完善 H2 hook 全模块覆盖 + `atexit` finalize（已实现）。

**禁止：** 强写 B71/15D/UI_MODE、伪造 helper 返回、跳过 stream、清空 queue、按 PC 固定平台返回值。

**完成型下一目标：**

```text
0x2F68E4 clean return (LR→0x2E4062)
→ 0x2E4066
→ 0x2DADC4
→ resource / DispUpEx
```

当前阻塞点：**`0x30A0CC` 内环在 B60=0x3D 处零进展** — 需查字段解析退出谓词与 nested code=5 outstanding 的交互，**不是** 10138/10132 合同缺口。
