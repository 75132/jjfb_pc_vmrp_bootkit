# JJFB PC vmrp direct replace v4：强制把 dsm_gm.mrp 替换成目标入口

## 当前结果结论

你的日志已经证明：

1. vmrp 能启动；
2. mythroad 根目录已经能扫描到 000_jjfb.mrp / 001_gwy.mrp / 002_gamelist.mrp；
3. 你点了 001_gwy.mrp 后，gwy 公共模块已经加载：
   - gwy/gamelist.mrp
   - gwy/cfg.bin
   - gwy/gui.mrp
   - gwy/font.mrp
   - gwy/reglogin.mrp
   - gwy/resmng.mrp
4. 但是没有 mock 收包，说明还没有进入实际联网检查/游戏启动阶段；
5. vmrp 命令行参数被忽略，它始终先启动 mythroad/dsm_gm.mrp。

## 新策略

既然 vmrp 固定启动：

```text
mythroad/dsm_gm.mrp
```

那就直接把目标入口复制成：

```text
mythroad/dsm_gm.mrp
```

这样 main.exe 一打开就会直接启动目标 MRP，不再经过 dsm_gm 列表。

## 三个启动目标

优先级：

```text
1. gwy      ：把 001_gwy.mrp 复制为 dsm_gm.mrp
2. gamelist ：把 002_gamelist.mrp 复制为 dsm_gm.mrp
3. jjfb     ：把 000_jjfb.mrp 复制为 dsm_gm.mrp
```

推荐先跑 gwy。

## 使用

解压到原 jjfb_pc_vmrp_bootkit 根目录覆盖。

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BOOT_GWY_V4.ps1
```

如果没有进入联网/列表，再分别跑：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BOOT_GAMELIST_V4.ps1
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BOOT_JJFB_V4.ps1
```

恢复原启动器：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_DSM_GM.ps1
```

## 回传

每次跑完发：

```text
logs/direct_replace_feedback_*.zip
```
