# v50 GWY Launcher Shim 实施报告

生成：2026-07-17

## 1. 本轮目标

按最新文档纠偏：停止 v49 的 `ui_mode=0x45 / AC8 / progress` 主线，建立最小 GWY Launcher Shim，模拟冒泡网游外壳执行 cfg index=36 的 `startGame/runapp`。

## 2. 查明的两个启动契约错误

### 2.1 入口仍是旧替代文件

原代码虽然传入 cfg36 参数，但实际启动：

```c
filename = "dsm_gm.mrp";
```

`dsm_gm.mrp` 只是旧实验中复制的 `jjfb.mrp`，会绕过/掩盖 `gwy/jjfb.mrp` 的真实目录关系。v50 launcher mode 已改为：

```c
filename = "gwy/jjfb.mrp";
```

### 2.2 把 nextid 当成 APPID

原 host code=8 appInfo 写死：

```text
id=482, ver=1
```

但原始 `jjfb.mrp` 头部实际为：

```text
APPID=400101
APPVER=12
```

`482` 是 cfg index=36 的 `nextid`，不是 MRP APPID。v50 改为从 MRP header offset 192/196 读取 big-endian APPID/版本，失败才回退 `400101/12`。

## 3. 修改文件

```text
runtime/vmrp_src_build_v27/vmrp-master/header/fileLib.h
runtime/vmrp_src_build_v27/vmrp-master/fileLib.c
runtime/vmrp_src_build_v27/vmrp-master/vmrp.c
runtime/vmrp_src_build_v27/vmrp-master/bridge.c
RUN_V50_GWY_LAUNCHER_MODE.ps1
scripts/v50_gwy_resource_audit.py
scripts/v50_analyze_launcher_log.py
README/01_HANDOFF_CONTEXT/LATEST_DIRECTION_v50.md
docs/HANDOFF.md（顶部增加 v50 覆盖通知）
```

## 4. 文件路径实现

launcher mode 下优先解析到：

```text
JJFB_MYTHROAD_ROOT=.../runtime/.../mythroad/240x320
JJFB_GWY_ROOT=.../runtime/.../mythroad/240x320/gwy
```

映射覆盖：

```text
gwy/jjfb.mrp
/gwy/jjfb.mrp
mythroad/gwy/jjfb.mrp
mythroad/240x320/gwy/jjfb.mrp
jjfbol/*
gifs/*
save/*
sound/*
```

canonical 路径不存在后才回退进程 CWD；创建文件时仍选 canonical 目标。

## 5. 新日志

```text
[JJFB_GWY_LAUNCH]
[JJFB_GWY_ROOT]
[JJFB_STARTGAME]
[JJFB_CFG36]
[JJFB_FILEOPEN]
[JJFB_FILEOPEN_MISS]
```

FILEOPEN 日志包括 guest、host、mode、handle、PC、LR。

## 6. 静态验证

### 6.1 C 语法

用项目自带 Unicorn 头进行 `fileLib.c / vmrp.c / bridge.c` 静态编译检查：无新增 error。

64 位 Linux 检查只出现原工程已有的指针宽度 warning；正式 runner 使用 mingw32 `-m32`。

### 6.2 C 路径函数实测

直接编译并调用修改后的 `my_resolve_path()`：

```text
gwy/jjfb.mrp          -> canonical .../240x320/gwy/jjfb.mrp             exists=1
jjfbol/default.mrp     -> canonical .../240x320/gwy/jjfbol/default.mrp   exists=1
/gwy/jjfb.mrp         -> canonical .../240x320/gwy/jjfb.mrp             exists=1
cfg.bin                -> canonical .../240x320/gwy/cfg.bin              exists=1
mrc_loader.ext         -> canonical host candidate                       exists=0
```

最后一项为预期：`mrc_loader.ext` 在 `jjfb.mrp` 内部，由 MRP loader 读取。

### 6.3 资源审计

已生成：

```text
reports/v50_gwy_resource_tree.md
reports/v50_file_path_mapping.md
```

资源树共 1551 个文件；`jjfb.mrp` 内部解析到 50 个条目，其中：

```text
start.mr       offset=1921   length=1514
mrc_loader.ext offset=3458   length=219
robotol.ext    offset=231594 length=161178
```

## 7. 被证伪/停止的假设

1. “注入 cfg36 参数就等于模拟了 GWY 启动”——错误；真实入口和资源根仍未建立。
2. “nextid=482 可以作为 appInfo.id”——错误；MRP APPID 是 400101。
3. “顶层找不到 mrc_loader.ext/robotol.ext 就是缺文件”——错误；两者在 `jjfb.mrp` 内部。
4. “继续推 UI 门控能代表进入游戏”——不再作为正式路线。

## 8. 当前 blocker

当前环境不是 Windows/MSYS2 mingw32，无法产出真实运行日志。因此尚未确认：

```text
canonical gwy/jjfb.mrp 是否被实际打开；
首次资源 FILEOPEN_MISS 是什么；
loader 在新 root 下是否仍能 mrc_init ret=0；
后续是否进入游戏自身网络流程。
```

## 9. 下一步最小任务

在项目根运行：

```powershell
.\RUN_V50_GWY_LAUNCHER_MODE.ps1 -Seconds 25
```

只根据生成的 `reports/v50_gwy_launcher_run_result.md` 决定下一步：

- 有 `FILEOPEN_MISS`：只修第一个真实路径/资源缺口；
- 无 miss、loader 失败：审计 `_strCom 601/800/801`；
- loader 成功：进入原始游戏自身网络阶段，继续遵守“不改游戏逻辑”。
