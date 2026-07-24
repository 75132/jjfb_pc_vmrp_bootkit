# Task 9: Field Length / Cursor Provenance and Contract Repair

**Date:** 2026-07-25  
**Runs:** `fs9_A_20260725_020457` (contract=0) / `fs9_B_20260725_020327` (contract=1)  
**Baseline:** Task 8 `fp8_B_20260725_013726`  
**Primary verdict:** `FIELD_PARSER_CONTRACT_REPAIRED`  
**Repair domain:** Scheme C — Path-A framing inner **binary copy** @ `0x2E4ECA`

---

## Core answer

> **白屏根因不是把 `r5` 改成 `0x10`，而是 Path-A framing 在 `0x2E4ECA` 的“memcpy”根本没有把 `BE(-1)` 写入 4 字节 inner。**  
> Guest import 解析到 DSM `0x804A8→0xA24FC`（非 memcpy），inner 保持 `0x10132` 零填充；`0x308D98` 读到 tag=`0` 后继续；`0x30A0CC` 在 cursor=4 处越界读到相邻堆上的 `"st"` → `r5=0x7374`。

| Q | A |
|---|---|
| `0x30A0EA` 应从哪读 length？ | **`stream_base + cursor_index` 的 BE u16**；写入在 **`0x30A0E8`**，`0x30A0EA` 是 `CMP r5,#0` |
| 正确 field length？ | Empty body：**无字段** — inner 应为 **`FF FF FF FF`（BE −1）**；`0x10` 是 entry+8 容量残差，不是 length |
| `"stat"` 是什么？ | **4 字节 inner 之后的相邻堆垃圾**，不是字段名 |
| heap/cursor 为何错？ | 拷贝未发生 → tag=0 → cursor 推进到 4 → OOB |
| 第一次偏离指令？ | 缺失的 memcpy 写 @`0x2E4ECA`；第一次错误读 @`0x30A0DA`（cursor=4） |
| `0x10132` 错了吗？ | **否** — size-malloc 正确；零填充是预期前置状态 |
| 修复域？ | **copy（Scheme C）**，非 producer / parser advance / 10132 |
| `0x2F68E4` 返回？ | **是**（B） |
| `0x2E4066` / `0x2DADC4`？ | **是 / 是**（B） |
| 窗口？ | 仍可能白屏（下游未修）；本轮已越过 helper 阻塞 |
| 第一个新自然行为？ | **`0x2DADC4` EVENT_GATE_INIT_ENTER** |

---

## 一、Length 数据流（已证明）

```text
0x30A0CC  r0=stream_base  r1=state
0x30A0D8  LDR  r0,[r4]              cursor_index = [state]
0x30A0DA  LDRB r1,[r6,r0]           lo = stream[cursor]
          STR  advanced cursor
0x30A0E0  LDRB r2,[r6,r0]           hi = stream[cursor]
0x30A0E2  LSLS r1,#8
0x30A0E6  ORRS r1,r2
0x30A0E8  ADDS r5,r1,#0             ★ BE u16 → r5
0x30A0EA  CMP  r5,#0                （非写入）
```

**BAD（A）现场：**

```text
base=0x2829E8  cursor_idx=4  len_ea=0x2829EC
raw = 0x73 0x74  → BE 0x7374 = 'st'
capacity(entry+8)=4  → 读已越界
```

---

## 二、真实 framing 布局

Helper `0x2F68E4` 记录循环：

```text
[BE u32 tag]              via 0x308D98   ; tag==-1 → 退出
[BE u16 len][bytes][NUL]  via 0x30A0CC   ; str1
[BE u16 len][bytes][NUL]  via 0x30A0CC   ; str2
[BE u32][BE u32]          via 0x308D98
→ malloc(0x10) pack → 0x312A60 publish
```

Empty Path-A body（`with_rec=0` / `platform_101ab_fill_path_a`）：

```text
inner (size=4) = FF FF FF FF
```

→ 第一次 `0x308D98` 得 −1 → helper 立即返回 → `0x2E4066` → `0x2DADC4`。

`"stat"` / `0x10` / `0x0C` 出现在 PAH 32 字节 dump 中，是 **inner 容量之外** 的相邻字，不是合法 record 体。

---

## 三、Path-A inner copy 闭环

| 阶段 | 证据 |
|---|---|
| 101AB fill | `src@0x6AD12B = 0xFFFFFFFF`（payload 内 BE −1） |
| malloc inner | `0x10132(size=8)→user 0x2829E8`，零填充 |
| framing BLX | `0x2E4ECA` r0=dest r1=src r2=4 → DSM `0x804A8` **非 memcpy** |
| A contract=0 | dest_before=0 dest_after=**0** repaired=0 |
| B contract=1 | dest_before=0 dest_after=**0xFFFFFFFF** repaired=1 |

**`0x10132` 未违反合同**（size-malloc + header）。违规的是 framing 导入的“拷贝”例程。

修复策略（`JJFB_FIELD_STREAM_CONTRACT`，默认 ON）：

```text
在 0x2E4ECA 执行前 host binary memcpy(dest,src,n)
不跳过 BLX（跳过会导致 R9 停在 DSM → 0x312A60 list=0 fault）
```

---

## 四、A/B 回归

| 项 | A contract=0 | B contract=1 |
|---|---|---|
| FSC repair | 0 | ≥1 |
| inner 首字 | `00000000` | `FFFFFFFF` |
| r5=0x7374 | **是** | **否** |
| FP 长拷贝环 | **是** | **否** |
| parser calls | ≥1 BAD | 0（−1 即退出） |
| `0x2F68E4` return | 否 | **是** |
| `0x2E4066` | 否 | **是** |
| `0x2DADC4` | 否 | **是** |

---

## 五、分类

```text
HEAP_COPY_SOURCE_OFFSET_WRONG     ✅（更准确：framing copy 未写入）
FIELD_DATA_POINTER_USED_AS_LENGTH_POINTER  ✅（OOB "st" 当 length）
FIELD_PARSER_CONTRACT_REPAIRED    ✅
BINARY_STREAM_TREATED_AS_C_STRING ❌（非 10132 strdup 路径）
PLATFORM_10132_COPY_LENGTH_WRONG  ❌
```

---

## 六、成功标准

| 标准 | B |
|---|---|
| 正确 length 来源已证明 | ✅ BE u16 @ base+cursor |
| cursor 第一次错位点 | ✅ 缺拷贝后 cursor=4 OOB |
| 修复在正确合同边界 | ✅ `0x2E4ECA` binary copy |
| r5 不再读 "st" | ✅ |
| 字段拷贝不越界 | ✅ 无 0x30A100 环 |
| `0x30A0CC` / helper / `0x2E4066` / `0x2DADC4` | ✅（empty 路径不进 parser） |
| trace off 行为 | 默认 FSC=ON |

Logs: `out/field_stream_task9/{A,B}/`  
Env: `JJFB_FIELD_STREAM_CONTRACT=0|1`（默认 1）+ `JJFB_FIELD_PARSER_TRACE=1`
