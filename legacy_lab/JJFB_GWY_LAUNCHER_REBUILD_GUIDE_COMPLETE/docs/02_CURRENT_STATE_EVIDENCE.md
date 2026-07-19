# 当前上传包的证据与可信结论

## 1. 资源与源码现状

本次上传包解压后约 596MB，包含：

- 完整 GWY 资源树；
- 原始 vmrp 源码；
- 已修改 vmrp 源码；
- Windows runtime；
- v50–v86 实验脚本与日志；
- 历史抓包/替换/直启实验；
- Mythroad 开发资料。

当前活动资源根为：

```text
game_files/mythroad/320x480/gwy
```

该目录在两层深度统计中有 1217 个文件；完整清单见 `evidence/gwy_resource_inventory.csv`。

## 2. 关键 MRP 结构已实测

### `gbrwcore.mrp`

```text
start.mr       stored 1018 / decoded 2490
reg.ext        stored 638  / decoded 684
gbrwcore.ext   stored 98264 / decoded 147196
```

### `gbrwshell.mrp`

含 `start.mr`、`reg.ext`、`gbrwshell.ext` 及外壳图形资源。

### `gamelist.mrp`

含：

```text
start.mr
gamelist.ext
reg.ext
cfg.bin
游戏列表/下载界面 BMP/GIF
```

### `vdload.mrp`

含 `vdload.ext`，其中可见旧下载协议域名和 HTTP 路径。该证据说明它是外壳更新层，而不是 JJFB 游戏核心。

### `jjfb.mrp`

原始 SHA-256：

```text
52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036
```

关键成员：

```text
start.mr          stored 1514 / decoded 3787
mrc_loader.ext    stored 219  / decoded 232
robotol.ext       stored 161178 / decoded 253420
reg.ext           stored 1381 / decoded 2472
20 个左右游戏模块 EXT
原始 slogo/loadingbar/bar/textbar 等资源
```

完整结构见 `evidence/key_mrp_manifest.md`。

## 3. cfg index 36 已从原始字节再次核对

在当前 cfg 中的经验记录位置：

```text
1024 + 36 × 272 = 10816
```

该 framing 目前只对上传样本的 index 36 做了字节级验证，不能提前宣称为通用 cfg 规范。

在该记录中可以直接读到：

- `ng_jjfb.gif`；
- `napptype=12` 候选字节；
- 固定 UTF-16BE 标题切片 `风暴(火爆公测)`；
- 观察到的 24-bit BE `nextid=482`；
- 观察到的 24-bit BE `ncode=512`；
- `narg=0`、`narg1=1`；
- `gwy/jjfb.mrp`。

详情见 `evidence/cfg_index36_record.md`。

注意：记录内的 4 字节 `g:u2` 不是有效 `sdk_key.dat`。当前有效 key 是根据平台 identity 生成的 48 字节二进制。

## 4. 当前启动链中真正可复用的成果

日志已经证明以下链路能够工作：

```text
canonical VFS path
→ gwy/jjfb.mrp
→ start.mr (1514)
→ sdk_key.dat canonical path
→ mrc_loader.ext (219)
→ robotol.ext (161178)
→ robotol helper 注册
→ version(6)
→ appInfo(8)
→ mrc_init(0) ret=0
```

这意味着：

- MRP archive 基础读取可用；
- gzip 解压可用；
- SDK identity/key 生成逻辑可用；
- Unicorn 运行 native EXT 的基础能力可用；
- `_strCom`/EXT 调用桥接已有可复用证据；
- `sendAppEvent` 已观察到多个平台注册请求；
- timer、文件、显示、网络桥接有部分实现。

## 5. 当前链路中不应继续沿用的部分

当前 runner 启用或默认包含：

```text
JJFB_MRC_RESUME_AFTER_INIT=1
JJFB_FAMILY_APP2_AFTER_INIT=1
JJFB_V64_ENQUEUE_ONCE=1
JJFB_FAMILY_C0_AFTER_B71=1
```

当前 `bridge.c` 还包含大量：

- JJFB 固定 PC hook；
- ERW 固定偏移探针；
- B70/B71/AC8/progress 路线；
- 画面修复与 host fallback；
- 人工 family/event 生产；
- guest 状态 heal；
- 版本号不断叠加的实验分支。

这些必须进入 `legacy_lab`，不能直接复制到新核心。

## 6. 重要修正：现有日志中的“startGame 等价”只是最低层调用

当前日志写：

```text
[JJFB_STARTGAME] startGame/runapp equivalent=bridge_dsm_mr_start_dsm
```

这只能证明“目标 MRP 被启动”，不能证明完整复刻了 GWY 外壳的 `startGame/runapp`。

完整的外壳启动契约至少还包括：

- cfg 记录解析；
- 资源根与命名空间；
- app identity；
- SDK key；
- EXT 主模块解析；
- platform table；
- 生命周期顺序；
- 注册式 callback/timer/event scheduler；
- pause/resume/foreground/background 语义；
- 网络与持久化服务。

因此新工程不能把“调用 `start_dsm`”当作最终 launcher 实现。

## 7. 已观察的平台注册调用

当前日志中至少出现：

```text
0x10102  注册 family handler
0x10113  请求/写入图形函数指针
0x10120  注册 ext chunk/handler
0x10130  大块内存/平台服务请求
0x10140  注册周期或主 handler
0x10162  注册/分配相关服务
0x10165  注册 enqueue handler
0x10180  获取用户/设备信息
0x10800  平台能力调用
0x1      普通平台探针/调用
```

完整样本见 `evidence/observed_platform_calls.csv`。

这些调用应在新架构中变成：

```text
PlatformRegistry + Scheduler + typed service handlers
```

而不是继续在一个巨型 `br_jjfb_sendAppEvent()` 中用固定地址和 if/else 推进。
