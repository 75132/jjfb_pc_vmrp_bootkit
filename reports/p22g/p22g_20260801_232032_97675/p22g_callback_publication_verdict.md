# P22G-CLEAN callback publication verdict

## Bottom line

**Class: F**

```text
farthest=HELPER_INVOKED
missing_transition=HELPER_INVOKED
→ expected producer=upstream_init_chain
→ via method/event=?
→ missing host contract=no callback publish; farthest=HELPER_INVOKED
```

## Identity

```
source commit：3a1fdfa12492827ce1a183af464001d7c6398841
main.exe SHA：4a671f3ce58df6333e9e7662bbec452cf21a48a12937a9cc9ffd9cab26b4f25d
gamelist.ext SHA：70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime image SHA：ed984d85451a7d7b8e3afe2b0595f12e0a3cb778d17a7d10143e91f4488c8fb6
runtime base：0x2D4364
module id：0x4
ERW：0x682B8C
P：0x2AC8EC
generation：2
package owner：gamelist.ext
```

## PASS answers

```
source commit：3a1fdfa12492827ce1a183af464001d7c6398841
main.exe SHA：4a671f3ce58df6333e9e7662bbec452cf21a48a12937a9cc9ffd9cab26b4f25d
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

初始化生命周期最远状态：HELPER_INVOKED
第一个缺失的状态转换：HELPER_INVOKED
负责该转换的 Guest 函数：upstream_init_chain
阻断分支：n/a
实际操作数：n/a
自然生产者：upstream_init_chain

是否修改 Guest：NO
是否注入事件：NO
是否 Host 调用 callback：NO
是否启用旧 headless/FAST：NO
当前唯一门锁：Class F — no callback publish; farthest=HELPER_INVOKED
下一处最小通用修复：restore natural HELPER_INVOKED contract (observe-only this round)
stop_reason：stable_idle
fire_ext_n：8
gl_insn_n：7370
```
