# GWY 文件夹、MRP 和模块解析方案

## 1. MRP 基本格式

当前资源使用 `MRPG` 容器。已验证的基础布局：

```text
0x00  "MRPG"
0x04  第一数据位置相关字段（当前解析为 first_data_offset + 4）
0x08  MRP 总长度
0x0C  header/index 起始长度（常见 0xF0 = 240）
0x10  内部名（12 字节区）
0x1C  显示名（24 字节区，常见 GBK）
0x44  APPID LE
0x48  APPVER LE
0x4C  flags
0xC0  APPID BE
0xC4  APPVER BE
```

索引记录：

```text
u32le name_len_including_nul
byte[name_len] member_name
u32le member_offset
u32le stored_length
u32le reserved
```

成员数据通常为 gzip；也可能是 raw。

包内 `tools/mrp_inspect.py` 已实现只读解析和提取。

## 2. `reg.ext` 的作用

多个 MRP 中都含 `reg.ext`。字符串证据显示：

- `gbrwcore.mrp/reg.ext` 声明 `gbrwcore.ext`；
- `jjfb.mrp/reg.ext` 列出了 `robotol.ext` 和全部游戏模块 EXT。

新 resolver 应优先研究并利用 `reg.ext`，而不是写死“主 EXT 一定叫 cfunction.ext”。

推荐解析策略：

```text
1. 读取 MRP index；
2. 若有 reg.ext，提取/解析其模块名称序列；
3. 建立 logical module role → archive member；
4. start.mr 请求 cfunction.ext 时：
   a. 先精确查找；
   b. 再查 reg.ext 标记的主模块；
   c. 再应用 profile alias；
   d. 不修改 guest 字面量，不修改 MRP。
```

当前 `cfunction.ext → robotol.ext` 可以保留为 JJFB profile 的兼容规则，但实现位置应在 `ExtResolver`，而不是 `br_log` 中改 `ext_base+0xD4`。

## 3. GWY 模块角色

### `gbrwcore.mrp`

推定角色：平台外壳核心、模块/游戏启动协调。应静态分析其：

- cfg/gamelist 调用接口；
- runapp/startGame 相关字符串或平台调用；
- 启动目标前设置的路径/identity/context。

### `gbrwshell.mrp`

推定角色：文件/目录 shell UI 与外壳资源。不是独立启动 JJFB 的必要运行依赖，除非分析证明它设置了通用平台上下文。

### `gamelist.mrp`

推定角色：列表、配置、下载/更新入口。它自身还内嵌一份 `cfg.bin`。需要比较：

```text
gwy/cfg.bin
vs
gamelist.mrp 内 cfg.bin
```

比较内容：哈希、记录数、index 36、版本字段。启动器应允许选择配置来源并记录来源哈希。

### `vdload.mrp`

更新下载插件。它不应进入日常 direct launcher。保留静态分析即可。

### `roomlist.mrp` 等公共模块

这些可能是 GWY 平台公共服务，不应默认全部加载。只有目标 MRP 或外壳启动契约明确请求时才加载。

### `jjfb.mrp`

目标游戏包。主链：

```text
start.mr
→ mrc_loader.ext
→ 主 EXT（reg.ext 指向 robotol.ext）
→ 游戏模块 EXT
```

## 4. cfg.bin 解析

当前 index 36 记录的已知布局足以建立第一版 parser，但未知字段必须保留为 raw：

```text
record_size = 272
record_base = 1024（当前资源版本观察值）
icon        @ 0x40
napptype    @ 0x57（候选，已与 12 一致）
legacy 4B   @ 0x58
UTF16BE title @ 0x5C
nextid LE32 @ 0x74
ncode LE32  @ 0x78
target path 在记录内搜索 gwy/*.mrp
```

正确实现不能假装所有 cfg 版本都一样。建议：

1. `CfgLayout` 版本结构；
2. 先用文件大小、magic/头字段和候选路径验证 layout；
3. 解析后执行 consistency check；
4. 未知字段输出 hex；
5. 若 layout 不匹配，拒绝启动并生成审计报告，而不是猜。

## 5. 资源根选择

当前资源采用：

```text
physical resource root = game_files/mythroad/320x480
native guest LCD       = 240×320
host window            = 320×480 stretch
```

资源目录名与 guest LCD 尺寸不必一致。启动器必须把这两件事分开：

- `resource_pack_variant = 320x480`
- `guest_display = 240x320`

禁止因为目录叫 320x480 就改 guest 坐标系统。

## 6. VFS 命名空间

所有 guest 路径规范化为 `/`，剥离允许的逻辑前缀：

```text
mythroad/gwy/jjfb.mrp → <root>/gwy/jjfb.mrp
gwy/jjfb.mrp          → <root>/gwy/jjfb.mrp
mythroad/sdk_key.dat   → <root>/sdk_key.dat
```

路径规则：

- 禁止 `..` 逃逸；
- Windows 大小写不敏感但 trace 保留 guest 原文；
- 只对明确的 legacy prefix 做映射；
- 资源写入重定向到单独 writable overlay，不直接污染 canonical source；
- source tree 只读，runtime overlay 可写。

建议：

```text
canonical_root/   原始资源，只读
runtime_overlay/  save/cache/downVersion 等写入
```

读取顺序：overlay → canonical；写入只到 overlay。
