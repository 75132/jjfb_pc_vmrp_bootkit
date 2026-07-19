# JJFB PC vmrp 修正版：240x320 文件树扁平化

## 我看了你回传的包，问题很明确

不是没有机甲风暴文件，也不是网络 mock 的问题。

日志里已经显示：

```text
jjfb_exists=False
```

原因是你的游戏文件放在：

```text
game_files/mythroad/240x320/gwy/jjfb.mrp
game_files/mythroad/240x320/gwy.mrp
```

但 vmrp 自带启动器 `dsm_gm.mrp` 只扫描：

```text
mythroad/
```

也就是根目录。日志里它只看到：

```text
mythroad/atxllq.mrp
mythroad/box.mrp
mythroad/dsm_gm.mrp
mythroad/gwy/
...
```

它没有进入 `mythroad/240x320/` 里面扫描，所以你当然看不到机甲风暴/网游。

## 这个修正版做什么

它会把：

```text
game_files/mythroad/240x320/*
```

扁平复制到 vmrp 实际文件系统根目录：

```text
runtime/vmrp_win32/.../mythroad/
```

并且额外创建几个根目录入口：

```text
mythroad/gwy.mrp          来自 240x320/gwy.mrp
mythroad/jjfb.mrp         来自 240x320/gwy/jjfb.mrp
mythroad/gamelist.mrp     来自 240x320/gwy/gamelist.mrp
mythroad/gwy/             来自 240x320/gwy/
```

同时删除旧的：

```text
mythroad/app.cfg
```

让 vmrp 启动器重新扫描列表。

## 用法

把这个压缩包解压到你原来的：

```text
jjfb_pc_vmrp_bootkit/
```

目录里，覆盖同名文件。

然后在 `jjfb_pc_vmrp_bootkit` 根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_FLATTEN_AND_LAUNCH.ps1
```

启动后，vmrp 列表里应该至少出现这些之一：

```text
gwy / 冒泡 / 网游
jjfb / 机甲风暴
gamelist
```

优先点：

```text
1. gwy.mrp / 网游 / 冒泡入口
2. gamelist.mrp
3. jjfb.mrp
```

如果点 `jjfb.mrp` 直接失败，不代表没戏；因为它可能需要 gwyblink 上下文。重点先看 `gwy.mrp` 或 `gamelist.mrp` 能不能进入大厅/列表。

## 跑完发什么

运行后发：

```text
logs/flatten_feedback_*.zip
```

