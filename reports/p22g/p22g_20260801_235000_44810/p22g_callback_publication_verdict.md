# P22G-CLEAN callback publication verdict

## Bottom line

**Class: B**

```text
farthest=HELPER_REGISTERED
missing_transition=HELPER_INVOKED
→ expected producer=parent_Shell_handoff
→ via method/event=method_6_8_0_or_equiv
→ missing host contract=Shell→gamelist helper init handoff missing (no natural method 6/8/0)
```

## Identity

```
source commit：819ef645ae90ab7ee0cf4e16415af2b23b165d91
main.exe SHA：eaa48b6054540e178eee1b7673390472496e3937adaaba66251d02b1f135ccb3
gamelist.ext SHA：70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime image SHA：e510fe8795381f27e1ec49048f04ee94a486baf0ede0c9382825d2d606427ca8
runtime base：0x2D4364
module id：0x4
ERW：0x682B8C
P：0x2AC8EC
generation：2
package owner：gamelist.ext
```

## PASS answers

```
source commit：819ef645ae90ab7ee0cf4e16415af2b23b165d91
main.exe SHA：eaa48b6054540e178eee1b7673390472496e3937adaaba66251d02b1f135ccb3
gamelist.ext SHA：70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime base：0x2D4364
module id：0x4
ERW：0x682B8C
P：0x2AC8EC
generation：2
package owner：gamelist.ext

module map 是否完成：YES
module header entry 是否调用：YES
helper 是否注册：YES
helper 首次自然调用者：NONE pc=0x0
自然 method/opcode 序列：NONE

+0xF670 是否被写入 Guest：NO
写入 PC：0x0
目的地址/表槽：0x0
是否被间接调用：NO

+0x8CDC 是否被写入 Guest：NO
写入 PC：0x0
目的地址/表槽：0x0
是否被间接调用：NO

+0xD978 是否被写入 Guest：NO
写入 PC：0x0
目的地址/表槽：0x0
是否被间接调用：NO

初始化生命周期最远状态：HELPER_REGISTERED
第一个缺失的状态转换：HELPER_INVOKED
负责该转换的 Guest 函数：parent_Shell_handoff
阻断分支：n/a
实际操作数：n/a
自然生产者：parent_Shell_handoff

是否修改 Guest：NO
是否注入事件：NO
是否 Host 调用 callback：NO
是否启用旧 headless/FAST：NO
当前唯一门锁：Class B — Shell→gamelist helper init handoff missing (no natural method 6/8/0)
下一处最小通用修复：定位父级 Shell 向 gamelist helper 发起 method 6→8→0（或等价）的自然 handoff 合同；禁止 Host 直调 F670/8CDC/D978/10740
stop_reason：stable_idle
fire_ext_n：8
gl_insn_n：7368
```
