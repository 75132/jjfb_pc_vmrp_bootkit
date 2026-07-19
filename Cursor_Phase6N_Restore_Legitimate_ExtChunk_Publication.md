# Cursor 下一阶段说明：Phase 6N — Restore legitimate mrc_extChunk publication（快进实现路线）

> Phase 6M 已经把问题收口：`P+0x0C` 不是“没被碰”，而是初始化流程把 `mr_c_function_st` 清零后，没有任何合法流程把 `mrc_extChunk*` 发布回 `P+0x0C`。  
> `chunk_field_04=0` 是 `mrc_extChunk_st + 0x04 init_func` 没有自然来源，不等于 `P+0x0C` 本身。  
> 所以下一步不要再大范围 observe；直接进入 **合法 extChunk publication 恢复**，但必须 gated、可回滚、可审计，禁止写死游戏地址。

---

## 0. Phase 6M 结论

已确认：

```text
classification = CFUNCTION_PUBLICATION_SOURCE_ZERO
P+0x0C nonzero = no
dsm:cfunction.ext @ 0x94F04 对 P 做 zero-init，包括 P+0x0C = 0
host _mr_c_function_new 也会 memset P，属于 documented behavior
EXT_REGISTER chunk_field_04 = 0
chunk_field_04_writer = NONE_BEFORE_SELECT
natural extChunk magic candidates = 0
wxjwq 同样 zero-writer
```

关键解释：

```text
P+0x0C 不是 chunk_field_04。
P+0x0C = mr_c_function_st.mrc_extChunk*
chunk_field_04 = mrc_extChunk_st + 0x04 init_func
```

所以缺的不是 entry，不是 ABI variants，也不是 runapp：

```text
缺的是合法 mrc_extChunk 对象的 allocation/register/publication。
```

---

## 1. Phase 6N 目标

Phase 6N 目标：

```text
恢复平台层合法 mrc_extChunk publication：
在 _mr_c_function_new / module register / cfunction-reg flow 中，
为 native/ext module 创建或定位真实 mrc_extChunk，
并自然发布到 P+0x0C。
```

注意：

```text
这是平台 shim 恢复，不是游戏 patch。
不能随便 malloc fake 指针。
必须基于源码/文档/跨游戏共同契约。
```

---

## 2. 禁止事项

仍禁止：

```text
硬编码 P+0x0C = 某个猜测地址
把 P+0x0C 指到 ER_RW/DSM/helper
直接跳过 0x30CCF8
R9 promotion
force ui_mode
AC8/progress
修改 jjfb.mrp / robotol.ext / wxjwq 游戏逻辑
```

允许：

```text
查源码里的 mr_c_function_st / mrc_extChunk_st 定义
恢复平台 extChunk allocator/register
按模块创建 platform-owned extChunk
将已创建的 legitimate extChunk 指针发布到 P+0x0C
补 extChunk slots 对应的真实平台 API dispatcher
用 jjfb/wxjwq/gbrwcore 跨目标验证
```

---

## 3. 不要再拖的方向

Phase 6N 不要再做：

```text
entry ABI variants
继续 runapp/gamelist 追踪
继续 UI/图形
继续扩大多游戏样本
继续只观察 P+0x0C 是否自然出现
```

原因：

```text
entry 已经命中；
zero-init 已确认；
没有 publication object 是当前唯一 blocker。
```

---

## 4. 6N-A：源码/头文件合同定位，限时完成

优先搜索源代码：

```text
mrc_extChunk
mr_c_function_st
extChunk
init_func
check magic 0x7FD854EB
MR_LOAD_C_FUNCTION
_mr_c_function_new
mr_c_function_load
```

必须输出：

```text
reports/phase6n_extchunk_contract_source.md
```

字段：

```text
struct name
field offset
field meaning
source file
evidence level: DOCUMENTED / CROSS_TARGET / HYPOTHESIS
```

最低需要确认：

```text
mr_c_function_st +0x0C = mrc_extChunk*
mrc_extChunk_st +0x04 = init_func
mrc_extChunk_st 是否有 magic/check field
mrc_extChunk_st +0x28 是什么 slot 或至少是有效 slot
```

如果源码里找不到完整定义，也要用现有报告中的 documented layout 建立最小合同，但标注为 `DOCUMENTED_PARTIAL`。

---

## 5. 6N-B：建立 platform-owned extChunk provider

新增一个独立模块，例如：

```text
src/runtime/ext_chunk_provider.c
src/runtime/ext_chunk_provider.h
```

不要散落在 shim 里。

### 5.1 数据结构

必须用显式结构和 offset assert：

```c
typedef struct JJFB_MrcExtChunk {
    uint32_t magic_or_check;   // if documented
    uint32_t init_func;        // +0x04 if documented
    uint32_t slot_08;
    uint32_t slot_0C;
    ...
    uint32_t slot_28;          // must be valid if guest reads +0x28
} JJFB_MrcExtChunk;
```

如果完整结构未知，先做：

```c
typedef struct JJFB_MrcExtChunk_Min {
    uint32_t slot[16];   // covers at least +0x00..+0x3C
} JJFB_MrcExtChunk_Min;
```

但必须满足：

```text
slot+0x04 非零，如果合同要求 init_func
slot+0x28 非零，因为 gbrwcore/robotol 会读取
```

注意：

```text
这个对象必须是 platform-owned extChunk，不是 fake per-game workaround。
```

---

## 6. 6N-C：publication 触发点

最小可行触发点：

```text
在 _mr_c_function_new 成功创建 P 后，
或者 EXT_REGISTER 成功后，
如果 module 有 helper / header_entry / P，
则为该 module 创建 extChunk 并记录到 module registry。
```

不能立刻无条件写 P，应该先经过 registry：

```text
module_id -> helper -> p_guest -> extChunk_guest
```

流程：

```text
_mr_c_function_new(helper, len=0x14)
  -> allocate/zero P
  -> register module P
  -> ensure module extChunk object
  -> publish extChunk pointer into P+0x0C
```

或者：

```text
EXT_REGISTER(module)
  -> ensure module extChunk object
  -> if P already exists: publish P+0x0C
```

需要同时支持顺序不确定：

```text
P 先出现，extChunk 后出现
extChunk 先创建，P 后绑定
```

---

## 7. 6N-D：publication 必须看起来是自然平台行为

日志必须是：

```text
[JJFB_EXTCHUNK_CONTRACT] struct=... source=...
[JJFB_EXTCHUNK_ALLOC] module=gbrwcore.ext module_id=3 guest=0x... size=...
[JJFB_EXTCHUNK_SLOT] module=gbrwcore.ext off=0x04 value=0x... meaning=init_func
[JJFB_EXTCHUNK_SLOT] module=gbrwcore.ext off=0x28 value=0x... meaning=...
[JJFB_EXTCHUNK_PUBLISH] module=gbrwcore.ext P=0x2AC8DC off=0x0C old=0x0 new=0x... reason=module_register
```

禁止出现：

```text
reason=fake
reason=force_fault_fix
reason=hardcode_for_jjfb
```

允许的 reason：

```text
reason=mr_c_function_new_contract
reason=ext_register_contract
reason=platform_publication_restore
```

---

## 8. 6N-E：slot +0x28 不要乱填

因为 gbrwcore 和 robotol 都会读：

```text
r0 = P+0x0C
read [r0 + 0x28]
```

所以 +0x28 必须指向合法 platform API / dispatcher。

最小要求：

```text
1. +0x28 非零；
2. 指向已经存在的 platform dispatcher / cfunction helper / documented slot；
3. 被调用时能记录参数并返回合理 status；
4. 如果不知道语义，先作为 observe dispatcher，不要伪造业务成功。
```

建议：

```text
[JJFB_EXTCHUNK_SLOT_CALL] off=0x28 pc=... args=... ret=...
```

如果 +0x28 被调用后出现新缺口：

```text
不要 skip；
分类 NEW_EXTCHUNK_SLOT_API。
```

---

## 9. 6N-F：运行顺序

### Step 1：gbrwcore only

```powershell
$env:JJFB_EXTCHUNK_PROVIDER="gbrwcore_only"
```

验收：

```text
P+0x0C old=0 new=nonzero
0x30CCF8 NULL+0x28 不再出现
若出现 NEW_EXTCHUNK_SLOT_API，记录 slot 和参数
```

### Step 2：gbrwcore + wxjwq

```powershell
$env:JJFB_EXTCHUNK_PROVIDER="gbrwcore_wxjwq"
```

确认跨目标一致。

### Step 3：jjfb

```powershell
$env:JJFB_EXTCHUNK_PROVIDER="gwy_shell"
```

看是否进入：

```text
_strCom 601/800/801
mrc_loader.ext
robotol.ext
mrc_init
```

---

## 10. 6N 成功标准

### 最低成功

```text
P+0x0C natural/platform-publication write appears:
[JJFB_EXTCHUNK_PUBLISH] old=0 new=nonzero
```

### 中级成功

```text
0x30CCF8 / 0x304580 NULL+0x28 fault 消失；
+0x28 slot 被真实调用或继续推进到下一 API 缺口。
```

### 高级成功

```text
_strCom 601/800/801 出现；
mrc_loader.ext / robotol.ext 自然链恢复；
mrc_init 出现。
```

### 可视化前兆

```text
jjfb 原始资源请求开始自然出现；
游戏自身启动/检查更新/登录前资源链推进。
```

---

## 11. 需要生成的脚本

```powershell
.\RUN_PHASE6N_RESTORE_EXTCHUNK_PUBLICATION.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
$env:JJFB_EXTCHUNK_PROVIDER="gbrwcore_only"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 12. 产物

```text
logs/phase6n_restore_extchunk_publication_stdout.txt
logs/phase6n_restore_extchunk_publication_report.txt

reports/phase6n_extchunk_contract_source.md
reports/phase6n_extchunk_provider_impl.md
reports/phase6n_pxc_publication_result.md
reports/phase6n_extchunk_slot28_trace.md
reports/phase6n_cross_target_wxjwq_check.md
reports/phase6n_next_fault_classification.md
reports/phase6n_verdict.md

packages/JJFB_phase6n_restore_extchunk_publication_pack_*.zip
```

---

## 13. 快速判定表

| 结果 | 解释 | 下一步 |
|---|---|---|
| `P+0x0C` 仍为 0 | provider 没挂到正确 P 或触发点错 | 修 registry/P binding |
| `P+0x0C` 非零但 `+0x28` fault | extChunk 地址不合法或结构布局错 | 修 extChunk struct |
| `+0x28` 被调用但新 API fault | 已过 publication，进入 slot API 补全 | Phase 6O |
| `_strCom 601/800/801` 出现 | 重大突破 | 继续 loader/mrc_init |
| 首屏资源请求出现 | 接近自然可视化 | 转资源/UI 验证 |

---

## 14. 给 Cursor 的一句话

**Phase 6M 已确认：`P+0x0C` 的 0 来自合法 zero-init，`chunk_field_04=0` 表示没有自然 `mrc_extChunk` 对象/`init_func` 被注册；不要继续 entry ABI/runapp/UI。Phase 6N 直接恢复平台层 legitimate extChunk publication：先从源码确认 `mr_c_function_st` 与 `mrc_extChunk_st` 合同，建立 `ext_chunk_provider`，在 `_mr_c_function_new`/`EXT_REGISTER` 后按模块创建 platform-owned extChunk，并把其 guest 指针发布到 `P+0x0C`，同时保证 `+0x04` 与 `+0x28` 是合法可追踪 slot。先 gbrwcore_only，再 wxjwq，再 jjfb；禁止硬编码 fake P+0xC、跳 fault 或改游戏逻辑。**
