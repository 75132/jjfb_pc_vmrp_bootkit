# 双轨路线：产品启动器与 GWY 协议研究必须分离

## 1. 为什么要双轨

目前路线偏离的根源，是把“为了理解平台而做的目标内探针”直接变成了产品启动逻辑。正确组织方式：

```text
产品轨（可交付）             研究轨（证据生产）
------------------          --------------------
解析 cfg/MRP                 静态分析 gbrwcore/gamelist
通用 VFS/ABI                 trace shell 的 runapp/startGame
EXT loader/resolver          横向比较多个游戏 MRP
注册式 scheduler             研究 0x101xx/event 语义
原始目标不改                 可用固定地址探针但只放 legacy_lab
```

研究轨的结果只有满足证据门槛后才能提升到产品轨。

## 2. 推荐的最终绕过方式

所谓绕过 GWY/gamelist 强制联网，不是去破解更新流程，而是：

1. 不把 `gbrwcore.mrp`、`gbrwshell.mrp`、`gamelist.mrp`、`vdload.mrp` 作为日常启动依赖；
2. 只读解析它们保留下来的 cfg、资源和启动契约；
3. 新启动器自己完成游戏选择后的“最后一公里”：descriptor、平台初始化、目标 MRP、lifecycle；
4. 游戏自己的服务器连接与外壳更新连接严格分离；
5. 旧游戏服务器是否在线，是启动器之后的独立问题。

这条路线比模拟整个 GWY UI/更新服务器更短、更稳定，也更接近用户要的“独立外壳启动器”。

## 3. 产品轨交付层级

### Level A：Inspector

能扫描资源、列出游戏、导出 descriptor，不执行。

### Level B：Loader

能原样加载目标 `start.mr` 和主 EXT，完成 init。

### Level C：Runtime Shell

通用 scheduler、display/input、timer/event，目标自然运行。

### Level D：Network-capable Runtime

提供网络 ABI，让游戏自然尝试连接；不保证旧业务服务器存在。

### Level E：Launcher UX

在核心稳定后才增加游戏列表 GUI、profile 管理、日志查看。

当前应先完成 A→B→C，而不是提前做 GUI 或游戏状态推进。

## 4. 研究轨问题树

### R1：cfg.bin 是怎样被 shell 消费的

要回答：记录边界、字段、选择索引、参数序列化。  
方法：静态 strings/disassembly + 多记录差分。  
产物：versioned cfg layout，不是硬编码 index 36 reader。

### R2：runapp/startGame 最终调用什么

要回答：目标路径/current MRP/param/root/lifecycle 的调用顺序。  
方法：在 `gbrwcore/gamelist` 平台边界 trace，禁止追 UI 内变量。  
产物：高层 launch sequence。

### R3：reg.ext 与主 EXT 解析

要回答：逻辑 `cfunction.ext` 如何映射到实际 module。  
方法：比较同 loader 的多个原始 MRP。  
产物：resolver rule + confidence。

### R4：平台注册服务

要回答：`0x10102/10140/10165/...` 参数 schema、ownership、lifetime。  
方法：官方头文件/简单 fixture/跨目标 trace。  
产物：typed service table。

### R5：生命周期

要回答：init 后何时 foreground/resume，pause/exit 如何送达。  
方法：最小自编 MRP fixture + shell trace。  
产物：通用 scheduler mapping。

## 5. 证据提升规则

| 证据 | 可进入 core 吗 | 处理 |
|---|---|---|
| 官方 SDK/source 明确 | 可以 | 写测试和文档 |
| 3+ 原始目标一致 | 可以，标明版本 | cross-target test |
| 仅 JJFB 日志 | 不作默认 | profile/trace-only |
| 固定地址探针推断 | 不可以 | legacy_lab |
| 为了画面推进的注入 | 不可以 | 删除/归档 |

## 6. 实验必须回答一个判别问题

好实验：

```text
对三个使用同版 mrc_loader 的原始 MRP，逻辑名 cfunction.ext 是否都由 reg.ext/主模块映射？
```

坏实验：

```text
再发一个 event 看画面会不会变。
```

每次研究任务必须先写：假设 A/B、观测点、哪种结果否定哪种假设、结果如何影响架构。
