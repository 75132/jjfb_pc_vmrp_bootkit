# 新对话开场提示词（直接复制）

我在做一个老 MRP/斯凯/冒泡网游项目，目标是让原始《机甲风暴》`gwy/jjfb.mrp` 在 PC vmrp 环境里正常启动运行。请先阅读我上传的交接包，尤其是：

- `01_HANDOFF_CONTEXT/MASTER_CONTEXT_HANDOFF_v2.md`
- `01_HANDOFF_CONTEXT/LATEST_DIRECTION_v49.md`
- `02_TECH_DOCS_v28_v49/Cursor_JJFB_v49_gwy_launcher_shim_direction.md`

最重要的方向纠偏：

**我们不是为了还原/重做机甲风暴 UI、动画，也不是改 `jjfb.mrp` 内部游戏逻辑。游戏部分不能动。我们是在仿冒泡网游/gwy 的启动游戏方式，绕过 gwy 外壳强制更新，补齐 `startGame/runapp` 对 `jjfb` 的启动上下文，让原始 `jjfb.mrp` 自己跑。**

当前正确路线：建立 `GWY Launcher Shim`，不要再主线 force `ui_mode=0x45`、AC8、progress_count。回到 `gbrwcore/gamelist/cfg.bin/startGame/runapp` 启动链，使用 cfg index=36 的参数启动原始 `gwy/jjfb.mrp`：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

已知平台兼容基础已经做过/验证过：`_mr_c_load`、`_strCom(601/800/801)`、`mrc_loader.ext`、`robotol.ext`、`mrc_init ret=0`、部分 timer/event、240×320、RGB565 `0xF81F` colorkey、obj=0 skip 等。

请不要把目标带回“仿 UI/动画”。如果看到 v43-v48 大量 UI/状态机分析，它们只是诊断过程，最新方向以 v49 为准：补冒泡网游/gwy 启动契约，而不是推 jjfb 内部 UI 状态。
