# Cursor 下一阶段说明：Phase 6K — Fix MRPGCMAP Entry Selection / Module Init Order

> Phase 6J 已完成，结论为 **Verdict B**：  
> 当前不是 runapp/gamelist 问题，也不是 jjfb/robotol 特有问题，而是 **MRPGCMAP 模块入口选择 / module init 顺序错误**。  
> gbrwcore.ext、jjfb、wxjwq 都表现为：P 的 `+0/+4/+8` 被写入，但 `+0x0C / mrc_extChunk` 从未自然发布，随后在 `r0+0x28` 处 fault。  
> Phase 6K 允许进入修复阶段，但只能修“合法入口 / 初始化顺序”，禁止伪造 `P+0xC`。

---

## 0. Phase 6J 结论摘要

### 0.1 已确认事实

```text
P = 0x2AC8DC
P+0x00 written = yes
P+0x04 written = yes
P+0x08 written = yes
P+0x0C written = no
nearest +0/+4/+8 writer = pc=0x30CADE, module=gbrwcore.ext
fault_pc = 0x30CCF8
fault_expr = r0 + 0x28
r0 = 0
```

### 0.2 入口选择异常

```text
observed_first_pc = 0x30CA96
expected/header entry = MRPGCMAP image + 8
entry_class = WRONG_ENTRY_SELECTION
dispatch nature = guest_callback_continuation
```

解释：

```text
当前不是从 MRPGCMAP module init entry 正常进入；
而是从 callback continuation / mid-function path 进入，
导致只写了 ER_RW metadata 的一部分，
没有跑到 mrc_extChunk publication。
```

### 0.3 多目标对照

```text
jjfb.start.mr == wxjwq.start.mr
jjfb.mrc_loader.ext == wxjwq.mrc_loader.ext

gbrwcore_jjfb:
  wrote +0/+4/+8/+C = yes/yes/yes/no
  fault = 0x30CCF8 / r0+0x28

gbrwcore_wxjwq:
  wrote +0/+4/+8/+C = yes/yes/yes/no
  fault = 0x30CCF8 / r0+0x28
```

这说明：

```text
这是 CROSS_TARGET 平台共同入口/初始化问题，不是 jjfb 单点问题。
```

---

## 1. Phase 6K 唯一目标

```text
修复 MRPGCMAP entry selection / module init order，
让合法 module init/publication path 在 callback continuation 之前运行。
```

不是：

```text
fake P+0xC
手动写 mrc_extChunk
R9 promotion
skip fault
force UI
继续追 runapp/gamelist
改 jjfb/robotol 游戏逻辑
```

---

## 2. 核心修复原则

### 2.1 先进入模块合法入口

对所有 MRPGCMAP native module，入口候选按：

```text
entry = image_base + 8
```

即跳过：

```text
MRPGCMAP 8-byte prefix
```

但注意：

```text
Thumb 执行状态必须正确；
日志里同时记录 raw_entry、thumb_entry、normalized_pc。
```

建议日志：

```text
[JJFB_MRPGCMAP_ENTRY] module=gbrwcore.ext base=0x... raw_entry=base+8 thumb_entry=... first_pc=...
[JJFB_MRPGCMAP_ENTRY_HIT] module=gbrwcore.ext pc=... expected=...
```

### 2.2 不要直接跳 callback continuation

当前错误路径类似：

```text
_mr_c_function_new / nested call
→ callback continuation
→ 0x30CA96
→ 0x30CADE 写 P+0/+4/+8
→ 未写 P+0xC
→ 0x30CCF8 fault
```

修复后应变成：

```text
load MRPGCMAP image
→ allocate/register module P
→ call module init entry image+8
→ publication routine natural runs
→ P+0xC natural write
→ then callback continuation / exported call
```

---

## 3. 6K 实施范围

### 允许

```text
修 MRPGCMAP loader entry selection
修 native module init order
修 cfunction/reg.ext primary publication flow
修 shell native init-before-continuation 顺序
补合法平台 API，使 module init 能自然返回
增加 watch/log/audit
```

### 禁止

```text
写 P+0xC = 某个猜测地址
构造 fake extChunk
把 P+0xC 指到 ER_RW/DSM/helper
skip 0x30CCF8
R9 promotion
force ui_mode=0x45
写 AC8/progress
host overlay UI
patch jjfb/robotol 逻辑
```

---

## 4. 推荐分步执行

### 6K-A：严格入口验证，先不修业务

新增环境：

```powershell
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="observe"
```

目标：

```text
只证明当前 first_pc 与 image+8 的差异；
打印每个 module 的 intended entry；
不改变执行。
```

输出：

```text
[JJFB_6K_ENTRY_AUDIT] module=gbrwcore.ext image_base=... intended_entry=... observed_first_pc=...
```

---

### 6K-B：只对 gbrwcore.ext 启用合法 entry-first

新增环境：

```powershell
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
```

目标：

```text
在 callback continuation 之前，先运行 gbrwcore.ext 的 MRPGCMAP entry。
```

验收：

```text
[JJFB_MRPGCMAP_ENTRY_HIT] module=gbrwcore.ext
[JJFB_P_WRITE] off=0x0C old=0x0 new=0x...
```

如果新 fault 出现在 entry 内：

```text
不要 skip；
记录新 fault 分类；
进入 observe-only 分类。
```

---

### 6K-C：扩展到 shell modules

若 6K-B 成功，再扩展：

```text
gbrwcore.ext
gbrwshell.ext
gamelist.ext
```

不要直接扩展到所有游戏模块，避免误伤。

---

### 6K-D：再跑 jjfb / wxjwq 对照

只在 shell module init 顺序修正后，再跑：

```text
jjfb
wxjwq
```

比较：

```text
P+0xC natural write
_strCom 601/800/801
mrc_loader.ext
mrc_init
first fault
```

---

## 5. 必须保留的动态 watch

### 5.1 P 字段 watch

继续记录：

```text
P+0x00
P+0x04
P+0x08
P+0x0C
P+0x10
```

日志：

```text
[JJFB_P_FIELD_WRITE] module=... off=0x00 old=... new=... pc=...
[JJFB_P_FIELD_WRITE] module=... off=0x04 old=... new=... pc=...
[JJFB_P_FIELD_WRITE] module=... off=0x08 old=... new=... pc=...
[JJFB_P_FIELD_WRITE] module=... off=0x0C old=... new=... pc=...
```

### 5.2 Entry / continuation 顺序

必须记录：

```text
[JJFB_ENTRY_ORDER] module=gbrwcore.ext state=loaded
[JJFB_ENTRY_ORDER] module=gbrwcore.ext state=entry_called
[JJFB_ENTRY_ORDER] module=gbrwcore.ext state=entry_returned
[JJFB_ENTRY_ORDER] module=gbrwcore.ext state=callback_continuation
```

目标是证明：

```text
entry_called < callback_continuation
```

而不是当前的：

```text
callback_continuation without entry
```

---

## 6. 静态重点

Phase 6J 已发现多处 `STR ..., #0xC`，所以 6K 不能说“没有 writer”。  
应重点判断：

```text
这些合法 writer 为什么没有执行？
它们是否位于 module entry / init path？
```

优先关注：

```text
gbrwcore.ext:
  0x3CB6..0x3CFE
  0x9E8A..0x9EC8
  0xA7F6..0xA82A
  0xE6D6..0xE6FC
  0x21BB8..0x21BFC

gamelist.ext:
  0xF078..0xF0BC

gbrwshell.ext:
  0x8D04..0x8D48
```

这些 clusters 有 `+0/+4/+8/+0xC/+0x10` 结构初始化特征。  
Phase 6K 要判断它们是否属于正确 init path。

---

## 7. 成功标准

### 最低成功

```text
observed_first_pc 不再是 callback continuation；
gbrwcore.ext 首次 guest PC 命中 MRPGCMAP image+8 或合法 entry thunk。
```

### 中级成功

```text
P+0x0C 出现自然写入；
writer pc/module 被记录；
0x30CCF8 NULL+0x28 fault 消失。
```

### 高级成功

```text
_strCom 601/800/801 出现；
mrc_loader.ext / robotol.ext 自然进入；
mrc_init 出现。
```

### 可视化前兆

```text
游戏自身资源请求开始出现；
jjfb 原始资源自然被读；
不再依赖 force UI/host overlay。
```

---

## 8. 如果 6K 出现新 fault

不要立刻修。  
先分类：

```text
NEW_ENTRY_FAULT
NEW_API_MISSING
NEW_FILE_MISS
NEW_ABI_FIELD_FAULT
NEW_EVENT_FAULT
```

如果是缺 API：

```text
可以补合法平台 API。
```

如果又是 field NULL：

```text
继续判断 source class，不允许猜写。
```

---

## 9. 新增脚本

```powershell
.\RUN_PHASE6K_FIX_MRPGCMAP_ENTRY_ORDER.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
$env:JJFB_PUBLICATION_AUDIT="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 10. 产物

```text
logs/phase6k_fix_entry_order_stdout.txt
logs/phase6k_fix_entry_order_report.txt

reports/phase6k_entry_order_change.md
reports/phase6k_p_extchunk_publication.md
reports/phase6k_gbrwcore_entry_result.md
reports/phase6k_cross_target_after_entry_fix.md
reports/phase6k_next_fault_classification.md

packages/JJFB_phase6k_fix_mrpgcmap_entry_order_pack_*.zip
```

---

## 11. 给 Cursor 的一句话

**Phase 6J verdict B：gbrwcore.ext / jjfb / wxjwq 都是 `WRONG_ENTRY_SELECTION`，当前从 callback continuation 进入，P 的 `+0/+4/+8` 被写但 `+0x0C` 从未自然发布，随后 `r0+0x28` fault。Phase 6K 允许修复 MRPGCMAP entry selection / module init order：在 callback continuation 前先运行合法 module entry（候选 image_base+8，注意 Thumb 状态和 PC 归一化），优先 gbrwcore_only，观察 `P+0x0C` 是否自然写入。禁止 fake P+0xC、R9 promotion、skip fault、force UI 或改游戏逻辑。**
