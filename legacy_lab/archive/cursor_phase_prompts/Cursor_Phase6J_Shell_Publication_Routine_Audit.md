# Cursor 下一阶段说明：Phase 6J — Shell Publication Routine Audit（结合多游戏共同契约）

> Phase 6H 已经把问题从“shell 是否执行”推进到了更精确的位置：  
> `gbrwcore.ext` 的 guest native code 已经执行，但在进入 `gamelist/lib.runapp` 前，shell 自己就因为 `P+0xC == 0` 发生 `*(NULL+0x28)` fault。  
> 所以下一步不要继续追 `lib.runapp`，也不要回到 UI/动画。  
> 下一步应该审计 **native/ext bootstrap 的共同 publication routine**：谁应该给每个 native ext 的 P 结构填 `mrc_extChunk`。

---

## 0. Phase 6H 最新结论

已确认：

```text
shell_native_exec_gate = open
guest PC 已进入 gbrwcore.ext
host_runapp_equivalent = no
gamelist = no
export_called = no
```

故障：

```text
fault_pc = 0x30CCF8
function_start = 0x30CCD0
expr = r0 + 0x28
r0 = 0x0
r3/P = 0x2AC8DC
P+0xC = 0x0
class = SHELL_LOADED_BUT_NO_EXTCHUNK
```

重要结论：

```text
gbrwcore.ext 已经执行；
但在任何 gamelist / lib.runapp 调用前，
shell native ext 自己就因为 P+0xC 为空而 fault。
```

所以 mid ladder 不是卡在 runapp 调度，而是卡在：

```text
native/ext 公共启动上下文 publication 缺失。
```

---

## 1. 结合多游戏静态分析后的新判断

多游戏静态对照已经显示：

```text
jjfb.mrp 与 wxjwq.mrp 的 start.mr 完全相同；
jjfb.mrp 与 wxjwq.mrp 的 mrc_loader.ext 完全相同；
gamelist/gbrwcore/gbrwshell 与游戏 mrp 都使用 cfunction.ext / _strCom / _mr_c_load；
多个游戏共享同一套 GWY/Mythroad native-ext bootstrap contract。
```

因此 `P+0xC` 很可能不是 jjfb 特有字段，而是：

```text
每个 native ext / cfunction ext 都需要的共同平台上下文字段。
```

当前 gbrwcore.ext 也卡在同一类 `P+0xC` 空指针，进一步说明：

```text
P+0xC provider 应在 gbrwcore/lib.runapp 之前；
甚至应在 shell native ext 入口前或入口初始化阶段完成。
```

---

## 2. Phase 6J 主目标

Phase 6J 只做一件事：

```text
定位并验证 P+0xC / mrc_extChunk 的 publication routine。
```

要回答：

```text
1. P+0xC 正常由谁写？
2. 是 _mr_c_function_new 写，还是 cfunction.ext/reg.ext 写？
3. 是 loadCode / _mr_c_load 后写，还是 native ext module entry 内写？
4. 为什么当前 P 结构其他字段有值，但 +0x0C 独为空？
5. gbrwcore.ext 和 jjfb/robotol.ext 的 P+0xC NULL 是否是同一个共同缺口？
6. 是否因为 entry selection 错误，跳过了真正会填 P+0xC 的 module init？
```

---

## 3. 不要继续追错方向

### 暂停

```text
lib.runapp / lib.startGame export 调度
gamelist post-update branch
UI / loadingbar / progress
R9 promotion
P+0xC fake
host_runapp_equivalent
```

原因：

```text
gamelist/runapp 尚未到达；
gbrwcore.ext 在更早阶段已经 fault。
```

### 继续保留

```text
gwy root/path
alias disabled
shell package open
guest PC trace
P+0xC watch
_strCom/mrc_init watch
多游戏对照
```

---

## 4. Phase 6J 必须重点分析的异常

### 4.1 P 结构其他字段被填，只有 +0x0C 为空

Phase 6H pre_fault P dump 显示：

```text
P+0x00 = 0x2B0D18
P+0x04 = 0x19A8
P+0x08 = 0x1
P+0x0C = 0x0
P+0x10 = 0x0
...
```

这说明：

```text
P 不是完全未初始化；
至少 ER_RW/start/len/type 等字段已经被部分填好；
唯独 mrc_extChunk 未发布。
```

6J 要找：

```text
填 P+0x00/+0x04/+0x08 的函数；
看它附近是否本应同时填 P+0x0C；
为什么跳过了 +0x0C。
```

日志：

```text
[JJFB_P_PROVIDER] write P+0x00 pc=...
[JJFB_P_PROVIDER] write P+0x04 pc=...
[JJFB_P_PROVIDER] write P+0x08 pc=...
[JJFB_P_PROVIDER] missing P+0x0C provider=...
```

---

### 4.2 `entry_class=WRONG_ENTRY_SELECTION` 不能忽略

6H 报告中有：

```text
first_pc = 0x30CA96
header   = 0x2EB7E8
entry_class = WRONG_ENTRY_SELECTION
```

同时还有：

```text
R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE module=gbrwcore.ext
```

这可能说明：

```text
当前进入了 gbrwcore.ext 的某个 bootstrap/code path，
但不是它的正确 module init entry；
或还没走完 cfunction.ext/reg.ext 的 module load initialization。
```

6J 必须判断：

```text
1. 0x30CA96 是什么？
2. 它是不是 helper path / callback / wrong thunk？
3. 正确 entry 是否应从 MRPGCMAP header 里的 entry offset 得出？
4. 是否应先执行某个 init/publication entry，之后才能调 0x30CCD0？
```

输出：

```text
reports/phase6j_entry_selection_vs_publication.md
```

---

## 5. 静态审计对象

重点文件：

```text
cfunction.ext
reg.ext
gbrwcore.ext
gbrwshell.ext
gamelist.ext
jjfb.mrp 内的 cfunction.ext / mrc_loader.ext
wxjwq.mrp 内的 cfunction.ext / mrc_loader.ext
```

重点函数/机制：

```text
_mr_c_function_new
_mr_c_load
_strCom
loadCode
MR_LOAD_C_FUNCTION
MRPGCMAP header parser
reg.ext primary
module entry
P struct allocation
ER_RW publish
mrc_extChunk publish
helper table publish
```

---

## 6. 静态扫描要求

### 6.1 找 P+0x0C writer

不能只 watch 动态，因为当前 writer 没执行。  
要静态找可能写 P+0x0C 的所有路径：

```text
STR rX,[rY,#0xC]
STR rX,[rY,#12]
基址 rY 由 P 指针传入
同一函数中也写 [P,#0], [P,#4], [P,#8]
```

输出：

```text
reports/phase6j_p_field_writer_xref.md
```

字段：

```text
module
pc/file_offset
instruction
writes offset
base register source
nearby writes to +0/+4/+8/+0xC/+0x10
executed?
why not executed?
```

---

### 6.2 找 `r0+0x28` ABI table 调用点

gbrwcore fault 和 jjfb fault 都是：

```text
r0 = P+0xC
call/use [r0+#0x28]
```

这说明 `P+0xC` 指向某个 ABI/helper/table。  
要找所有：

```text
LDR r?, [r0,#0x28]
LDR r?, [rx,#0x28] 且 rx 来自 P+0xC
```

输出：

```text
reports/phase6j_extchunk_abi_users.md
```

目标：

```text
确认 +0x28 是哪个平台函数；
看 gbrwcore 与 jjfb 是否使用同一 ABI helper。
```

---

### 6.3 MRPGCMAP header / entry 解析

输出：

```text
reports/phase6j_mrpgcmap_entry_decode.md
```

必须包含：

```text
module name
header magic
code_base
code_size
entry offset candidates
first executed PC
function_start at fault
whether entry selection matches header
```

尤其是：

```text
gbrwcore.ext
gbrwshell.ext
gamelist.ext
jjfb/cfunction.ext
wxjwq/cfunction.ext
```

---

## 7. 动态对照要求

### 7.1 gbrwcore.ext P 字段写入链

动态 hook 整个 P 结构：

```text
P+0x00 .. P+0x80
```

不要只看 +0x0C。

日志：

```text
[JJFB_P_FIELD_WRITE] off=0x00 old=... new=... pc=... module=...
[JJFB_P_FIELD_WRITE] off=0x04 old=... new=... pc=... module=...
[JJFB_P_FIELD_WRITE] off=0x08 old=... new=... pc=... module=...
[JJFB_P_FIELD_WRITE] off=0x0C old=... new=... pc=... module=...
```

目标：

```text
找出谁写了 +0/+4/+8，却没写 +0xC。
```

---

### 7.2 多游戏 minimal positive control

不要全面跑大分支，先做最小对照：

```text
target = gbrwcore.ext
target = jjfb.mrp
target = wxjwq.mrp
```

观察同一组指标：

```text
P+0x00/+0x04/+0x08/+0x0C writes
first fault pc
r0+0x28 use
entry_class
_strCom
mrc_loader
mrc_init
```

输出：

```text
reports/phase6j_minimal_cross_target_publication_compare.md
```

如果 `wxjwq` 与 `jjfb` 同样在 `P+0xC` 卡：

```text
说明 mrc_loader 类共同缺 publication。
```

如果 `gbrwcore` 与游戏都同样在 `P+0xC` 卡：

```text
说明 publication 是全平台共同缺口，不是游戏缺口。
```

---

## 8. 可能结论与下一步

### 结论 A：发现合法 publication routine，但当前没调用

下一步：

```text
Phase 6K: call/restore legitimate publication routine before shell native entry
```

仍禁止 fake pointer。

### 结论 B：entry selection 错，跳过 init

下一步：

```text
Phase 6K: fix MRPGCMAP entry selection / module init order
```

### 结论 C：P+0xC 应由 cfunction.ext/reg.ext primary 写

下一步：

```text
Phase 6K: restore cfunction.ext/reg.ext publication flow
```

### 结论 D：所有对照样本都缺 P+0xC，且找不到 writer

下一步：

```text
继续扩大多游戏/源码/旧 vmrp 文档对照；
仍不允许 fake extChunk。
```

---

## 9. 运行脚本

新增：

```powershell
.\RUN_PHASE6J_SHELL_PUBLICATION_AUDIT.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_PUBLICATION_AUDIT="1"
$env:JJFB_MULTI_TARGET_MIN_COMPARE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 10. 产物

```text
logs/phase6j_shell_publication_stdout.txt
logs/phase6j_shell_publication_report.txt

reports/phase6j_p_field_writer_xref.md
reports/phase6j_extchunk_abi_users.md
reports/phase6j_mrpgcmap_entry_decode.md
reports/phase6j_entry_selection_vs_publication.md
reports/phase6j_minimal_cross_target_publication_compare.md
reports/phase6j_publication_verdict.md

packages/JJFB_phase6j_shell_publication_audit_pack_*.zip
```

---

## 11. 成功标准

### 最低成功

```text
定位谁写了 P+0/+4/+8，确认为什么 +0x0C 没写。
```

### 中级成功

```text
找到 P+0x0C 的合法候选 writer / publication routine，或证明当前 entry/init 顺序跳过了它。
```

### 高级成功

```text
给出唯一下一阶段修复方向：restore publication routine / fix entry selection / restore cfunction-reg publication flow。
```

---

## 12. 给 Cursor 的一句话

**Phase 6H 已证明 gbrwcore.ext guest native 已执行，但在 gamelist/runapp 前就因 `P+0xC=0` 于 `0x30CCF8` 发生 `r0+0x28` fault；结合多游戏静态分析，`P+0xC` 很可能是 native/ext 共同平台 publication 字段，不是 jjfb 特有。Phase 6J 请暂停 runapp/gamelist，转为 Shell Publication Routine Audit：静态找 P+0x0C writer，动态记录 P+0/+4/+8/+0xC 字段写入链，解码 MRPGCMAP entry selection，比较 gbrwcore/jjfb/wxjwq 最小对照，定位合法 mrc_extChunk publication routine。禁止 fake P+0xC、R9 promotion、force UI 或改游戏逻辑。**
