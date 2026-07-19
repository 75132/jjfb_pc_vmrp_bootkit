# Cursor 开发说明 v48：不要继续“看起来能画”，改做 0x2EF86C 分支覆盖与事件码定位

> v47 结论很清楚：progress driver 能让 bar 画，但不能触发“检查更新/网络”字符串；说明现在不是画图链问题，而是 **0x45 启动检查状态机的某个分支没有执行**。  
> v48 目标：找出为什么 `0x2EFC40..0x2EFC80` 的 progress/AC8 写入路径没有走到，尤其要查 `0x306305/0x306344/0x30662C -> 0x2EF86C` 传入的事件码是否错或缺。

---

## 0. 当前结论

已经确定：

```text
1. loadingbar/bar/textbar 对象绑定是对的。
2. progress_count > 0 时，bar 动画链能画。
3. progress_count 从 0 线性驱动到 12，仍然不会进入“检查更新/网络”字符串。
4. AC8/progress_count/ui_mode 都没有 guest 自然写入。
5. 0x10140 handler = 0x306305，timer 活着。
6. 每 tick 反复进入：
   0x306344/0x30662C -> 0x2EF86C
   参数大致：
   r0=0x45, r1=0x13, r2=0xFFFFFFFF, r3=0x2B199C
7. 静态线索：
   imm_BA0 @ 0x2EFC58
   imm_AC8 @ 0x2EFC6C
   都在 splash 尾部附近，但 STR 路径未执行。
```

所以 v48 不要再重复：

```text
继续修 2EC6B0
继续调 loadingbar 坐标
继续手动 progress driver
继续看 UI 漂不漂亮
```

现在要做：

```text
精确回答：0x2EF86C 为什么没走到 0x2EFC40..0x2EFC80？
```

---

## 1. 最优先：做 0x2EF86C 的基本块覆盖图

不要只打印 SPLASH_ENTER。  
要给 `0x2EF86C..0x2EFD00` 做 basic-block / PC hit coverage。

### 输出

```text
reports/v48_2EF86C_block_coverage.md
```

内容必须包括：

```text
block_start
hit_count
last_lr
last_r0-r7
是否访问 AC8
是否访问 BA0
是否访问 progress_count
是否进入 0x2EFC40..0x2EFC80
分支跳到哪里
```

重点确认：

```text
0x2EFC40
0x2EFC58
0x2EFC6C
0x2EFC80
```

是否 hit。  
如果不 hit，要找最近一个把它跳过的 conditional branch。

### 日志例子

```text
[JJFB_2EF86C_COV] pc=0x2EFA9E hits=...
[JJFB_2EF86C_BRANCH] pc=... cond=... taken=... target=... r0=... r1=... flags=...
[JJFB_2EFC_GATE] not reached, last_branch=...
```

---

## 2. 反汇编 `0x2EFC40..0x2EFC90`，不要只说有 imm

v47 只说：

```text
imm_BA0@0x2EFC58
imm_AC8@0x2EFC6C
```

这还不够。v48 必须输出完整反汇编和伪逻辑：

```text
reports/v48_2EFC40_tail_disasm.md
```

内容：

```text
0x2EFC40..0x2EFC90 每条指令
寄存器来源
是否 STR / STRH / STRB
目标 offset 是什么
写入值来自哪个寄存器
前置 branch 条件是什么
谁调用/跳到 0x2EFC40
```

目标是确认：

```text
这里是否真的写 progress_count / AC8；
还是只是读 BA0/AC8 或计算地址。
```

不能继续把“有 imm_BA0/AC8”直接等同于 writer。

---

## 3. 事件码矩阵：r1=0x13 可能不是推进动画的事件

现在 dispatch 每 tick 看起来都是：

```text
r0 = 0x45
r1 = 0x13
r2 = 0xFFFFFFFF
r3 = 0x2B199C
```

`0x13` 可能只是某类 paint/timer 事件，不一定是 progress/update 事件。  
如果 0x2EF86C 的尾部 writer 只在特定 event code 下执行，那么一直 r1=0x13 就会空转。

### v48 必须做 event code scan

新增：

```powershell
.\RUN_V48_EVENTCODE_SCAN.ps1
```

扫描 r1 候选：

```text
0x00..0x30
以及已知 MRP 事件码/按键/定时器/刷新候选
```

每次只注入一个事件，保留：

```text
r0=0x45
r2/r3 使用当前真实值
ui_mode/state 不额外改
```

记录：

```text
event_code
是否进入 0x2EFC40..
是否写 AC8
是否写 progress_count
是否读检查更新字符串
是否 bar/textbar 动画变化
是否 ui_mode 变化
```

输出：

```text
reports/v48_eventcode_scan.md
```

如果某个 event code 能命中 0x2EFC40 或写 progress/AC8，说明不是图形问题，而是 timer/event 映射错了。

---

## 4. 0x10140/timer handler 要闭环到“事件码来源”

现在只知道：

```text
handler=0x306305
```

但还不知道：

```text
0x306305 每次被调用时为什么生成 r1=0x13？
r1=0x13 是谁传入的？
真正 timer / app event / refresh event 是否应该传别的 code？
```

v48 要追：

```text
0x10140 注册参数
handler=0x306305 调用参数
0x306305 -> 0x306344 -> 0x30662C 参数变换
r1/event_code 的来源
```

日志：

```text
[JJFB_10140_REG] handler=0x306305 raw args...
[JJFB_HANDLER_306305] enter r0-r7 sp0 lr
[JJFB_HANDLER_306344] event_code_before=...
[JJFB_DISPATCH_30662C] ui_mode=0x45 event=...
```

目标：

```text
确认当前平台 timer 是否把错误 event_code 喂给 robotol。
```

---

## 5. 对 0x2EFC40 tail 做一次“受控分支强制”验证

在弄清 branch 前，可以做一个最小实验，但必须标 probe：

```text
JJFB_FORCE_2EFC_TAIL=1
```

行为：

```text
当 PC 到达跳过 0x2EFC40 的前一条 branch 时，
临时强制进入 0x2EFC40 tail；
不直接写 progress/AC8；
让 guest 自己执行 tail。
```

观察：

```text
是否自然写 progress_count / AC8
是否进入检查更新字符串
是否 ui_mode 变化
是否崩溃
```

这比手动 progress driver 更有价值，因为它能验证 tail 代码到底是不是状态推进逻辑。

---

## 6. 检查更新字符串 xref 要扩大，但不要作为第一优先级

v47 的字符串静态扫描只扫了部分范围，而且直接搜 VA word 不够。

v48 需要：

```text
1. 全 robotol 映像范围扫描；
2. 同时扫描 VA、VA-BASE、字符串表 offset、近邻指针表；
3. 对 GBK 字符串附近的指针表做反向扫描；
4. watch 文本绘制函数 0x12340/文本路径是否有这些字符串对象。
```

输出：

```text
reports/v48_update_string_full_xref.md
```

但优先级低于 0x2EF86C 分支覆盖和 event code scan。

---

## 7. 保留当前正确修复，不再反复改

保留：

```text
240×320
2F9968=240
2F995C=320
0xF81F colorkey
obj=0 skip/no dirty
0x10134 返回 pixel ptr
loadingbar/bar/textbar object 绑定
```

不要再回退这些。

---

## 8. v48 成功标准

### 最低成功

```text
明确 0x2EF86C 哪个分支跳过了 0x2EFC40 tail。
```

### 中级成功

```text
找到一个 event_code 或 tail branch 条件，可以让 0x2EFC40..0x2EFC80 执行。
```

### 高级成功

```text
guest 自然写 AC8/progress_count 或进入检查更新/网络 UI 字符串路径。
```

### 游戏目标成功

```text
启动检查 UI/动画不靠 progress driver 自然推进。
```

---

## 9. 给 Cursor 的一句话

**v47 已证明 progress driver 只能让 bar 画，不能进入检查更新阶段；所以不要继续手动 progress，也不要继续修 UI。v48 请直接对 0x2EF86C 做基本块覆盖和分支日志，完整反汇编 0x2EFC40..0x2EFC90，扫描 r1/event_code 看是不是一直喂 0x13 导致 tail writer 不执行，并闭环 0x10140/0x306305 的 timer/event 参数来源。目标是让 guest 自己执行 progress/AC8 writer，而不是继续 host driver。**
