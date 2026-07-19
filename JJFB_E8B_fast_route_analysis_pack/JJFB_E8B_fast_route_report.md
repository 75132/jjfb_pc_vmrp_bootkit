# JJFB E8A 文件核验与最快实际路线

生成时间：2026-07-19T11:58:47

## 1. 已核验输入

- `jjfb.mrp` 与资源包 `240x320/gwy/jjfb.mrp` SHA-256 一致：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`。
- `wxjwq.mrp` 与资源包 `240x320/gwy/wxjwq.mrp` SHA-256 一致：`6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d`。
- `JJFB_E8A_forensics_delivery.zip` 不是纯原始输入，而是已经包含 post-E8A build、forensic 日志、静态分析与 probe 结果的交付包。

## 2. 阶段判断

E7 基线仍是：`lifecycle FIRE -> handler 0x30630D -> UC_ERR_INSN_INVALID @ 0x306338`。

E8A 交付包显示：Thumb LSB / Unicorn 入口问题已经被单点修复。post-fix 日志中：

- `UC_ERR_INSN_INVALID` 次数：0；
- lifecycle handler tick 1..9 均 `ok=1 end=stop_at_base uc_err=0`；
- entry `T=1`，fault/return 边界 `T=1`，`R9=0x2B1858` 保持不变；
- 当前还没有自然 `DRAW`，日志里仅有启动期 `REFRESH` 字样，不代表画面完成。

因此当前不应再做 “修 CPSR/SP/LR/参数” 的大改。旧 blocker 已过，下一步是 E8B：观察 handler 返回后真实触达的平台 API / 资源 / 绘图链。

## 3. 静态二进制证据

`robotol.ext` handler 映射：

```text
code_base = 0x2D8DF4
handler VA = 0x30630C
file_off = 0x2D518
fault VA = 0x306338
file_off = 0x2D544
file bytes @ fault = 63 D1 F8 F7 2B FC EF 48
```

该 fault bytes 按 Thumb 是合法指令，按 ARM 会变成非法；E8A probe 证明 Unicorn 1.0.2 使用 `uc_emu_start` 地址低位选择 Thumb，而不是只看 CPSR.T。

## 4. 附属资源包核验

`240x320/gwy/jjfbol/` 是 JJFB 的真实附属资源目录：

- 文件数：120；
- `.mrp` 资源包数：59；
- `.v` 版本文件与 `downVersion` 存在；
- 总体大小约 1.1 MB。

对 `robotol.ext` 中可见资源引用做了覆盖检查：

- 可解析文件/资源引用：86；
- 已由 `jjfb.mrp` 或 `gwy/jjfbol/*.mrp` 满足：81；
- 未直接满足：5，主要是运行期生成/缓存/商店文本，如 `cache/exdet.dat`、`bgappinf/appinf.dat`、`VipShop.txt`。

结论：这批附属资源包必须接入 VFS。后续如果出现资源下载/资源等待，优先检查 `gwy/jjfbol/` 是否被 runtime root 正确命中。

## 5. 一个必须立刻处理的路径风险

你上传的资源包根目录是：

```text
game_files/mythroad/240x320/gwy/
```

但 E7/E8A 日志里 runtime 绑定的是：

```text
game_files/mythroad/320x480
```

并且日志中曾打开 `system/gb16.uc2`，而你当前 240x320 包里只有 `system/gb12.uc2`。

这不是现在 UC_ERR 的根因，但会变成下一阶段资源/字体/画面问题的坑。最快做法：

1. 要么把当前资源包完整放到 runtime 实际使用的 root；
2. 要么把 profile/manifest 切到 `240x320`；
3. 要么在 GuestVFS 加只读 alias：`320x480 -> 240x320`，并给 `gb16.uc2` 明确 fallback 到 `gb12.uc2`，日志必须标出 alias。

## 6. E8A wxjwq 对照无效

`stage_e8a_wxjwq_forensic_stdout.txt` 仍然显示 `owner=robotol.ext`、handler `0x30630D`、同样的 `gwy/jjfb.mrp` 生命周期。

所以这不是干净的 wxjwq 对照，不能拿它证明 mmochat 成功/失败。下一阶段要修 runner 的 target/appid/cfg override，再跑真正的 `gwy/wxjwq.mrp`。

## 7. 最快实际路线：Stage E8B

### 目标

不要继续证明 handler 能不能跑，它已经能跑。现在要拿到第一处真实阻塞：绘图、资源、网络、输入/事件或平台返回值。

### 最小改动

1. 使用 E8A post-fix 的 `main.exe` 或把对应 one-line fix 合入当前源码。
2. 把 post-start loop 延长到 20~30 秒或 300~600 ticks。
3. 增加唯一平台调用 census，不要刷屏：按 `(code, app/family, caller_pc, ret)` 汇总。
4. 对 handler tick 内已经出现的 `0x1E209 / 0x9` 做专门追踪，但不要盲目改返回 1。
5. 加 `DRAW/REFRESH/bitmap/blit/fileopen/missing/network/connect` 汇总行。
6. 修正 `240x320/320x480` resource root 问题。
7. 重跑干净 wxjwq control。

### 当前最高优先级观察点

post-fix handler 每个 tick 都触达：

```text
GUEST_INDIRECT_CALL pc=0x30666A target=0x304559 arg0=0x1E209 arg1=0x9
JJFB_EXTCHUNK_SLOT_CALL r0=0x1E209 r1=0x9 ret=0x0
```

下一步要判断这个 ret=0 是“正常无事件/无输入”，还是“缺平台能力导致它不进入下一阶段”。

## 8. 给 Cursor 的下一条指令

```text
Stage E8B：基于 E8A post-fix，不再改 Thumb/CPSR/SP/LR/R0-R3。

已确认 E7 的 UC_ERR_INSN_INVALID @0x306338 是 Unicorn 1.0.2 对 uc_emu_start 地址低位的 Thumb 判定问题；E8A one-line fix 后 lifecycle handler 0x30630D tick 1..9 均 ok=1 stop_at_base，T=1，R9=0x2B1858 不变。

请做下一阶段最小观察：
1. 延长 post-start loop 到 20~30 秒或 300~600 tick。
2. 输出唯一平台调用 census：code/app(or family)/caller_pc/ret/count/first_tick/last_tick。
3. 专门追踪 tick 内 0x1E209/0x9：调用前后 R0-R3、返回值、调用者附近 Thumb 反汇编、返回值分支路径。不要先改 ret。
4. 汇总 DRAW、REFRESH、bitmap/blit、file open miss、resource path、network/connect/update 字样。
5. 修正资源根路径：当前用户资源是 game_files/mythroad/240x320，但旧日志绑定 game_files/mythroad/320x480；要么 profile 切 240x320，要么 VFS 增加 320x480→240x320 alias，并记录 alias 命中。gb16.uc2 如缺失需明确 fallback 到 gb12.uc2。
6. 重新实现干净 wxjwq runner，不允许 cfg36/appid 仍覆盖到 jjfb；wxjwq log 中 owner 必须是 mmochat.ext，否则判为 contaminated。

禁止：force UI、伪造 DRAW、跳过 handler、修改 MRP/EXT、盲目把 0x1E209 返回改成 1、回退到 21002/25B/gamelist。

最终 verdict 只能从以下选择：
DRAW_REACHED、RESOURCE_ROOT_MISS、RESOURCE_MEMBER_MISS、PLATFORM_1E209_GATE、NETWORK_CONNECT_WAIT、EVENT_INPUT_IDLE、WXJWQ_CONTROL_CLEAN、WXJWQ_CONTROL_CONTAMINATED、NEXT_PLATFORM_CODE_<code>。
```

## 9. 当前一句话

现在已经不是 “handler ABI fault” 阶段了；你手里的 E8A 包证明 Thumb 根因已过。最快推进是：先修资源 root 一致性，再长跑 post-start loop，抓唯一平台调用与第一处真实等待点。
