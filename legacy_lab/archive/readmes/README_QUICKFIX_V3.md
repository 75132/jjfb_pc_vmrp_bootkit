# JJFB PC vmrp quickfix v3：不再递归复制，只处理真实运行目录，并把入口放到列表最前面

## 本次日志结论

你的 v2 其实已经把文件放进去了，vmrp 日志里已经能看到：

- mythroad/gamelist.mrp
- mythroad/gwy.mrp
- mythroad/jjfb.mrp

问题不是“没有文件”，而是：

1. v2 扫到了很多 mythroad 根，复制太多，容易像卡住；
2. vmrp 仍然启动 dsm_gm.mrp 列表页，不会自动进入游戏；
3. 需要把 jjfb/gwy/gamelist 放到列表最前面，方便你手动点。

## 用法

解压到原来的 jjfb_pc_vmrp_bootkit 根目录，覆盖同名文件，然后运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_QUICKFIX_V3.ps1
```

打开 vmrp 后，列表最前面应该出现：

```text
000_jjfb.mrp
001_gwy.mrp
002_gamelist.mrp
```

优先点：

```text
001_gwy.mrp
002_gamelist.mrp
000_jjfb.mrp
```

如果点了有网络连接，logs/mock_*.jsonl 会记录。
