# JJFB v26：不走 start.mr，直接让 vmrp 启动 mrc_loader.ext

## v25 / v24 结论

现在已经确认：

```text
1. jjfb.mrp 能作为 dsm_gm.mrp 被 PC vmrp 启动；
2. main.exe 第 4 参数注入成功；
3. start.mr 的 sdk_key 错误分支已经能跳过；
4. 但 start.mr 正常流程卡在 _mr_c_load / hsman；
5. v24 跳过 line157 后多个目标全部没有 mrc_loader / robotol / 20000 / 21002。
```

所以继续 patch `start.mr` 已经意义不大。

## 新路线

`jjfb.mrp` 内部本来就包含：

```text
mrc_loader.ext
robotol.ext
bigworldmapmodule.ext
mainmenumodule.ext
...
```

而 vmrp 源码里原本支持：

```c
bridge_dsm_mr_start_dsm(uc, filename, extName, entry)
```

之前我们一直让 `extName = "start.mr"`。

v26 直接改成：

```text
filename = dsm_gm.mrp
extName  = mrc_loader.ext
entry    = cfg 参数
```

也就是：**绕过 start.mr，直接启动 C/EXT loader**。

## v26 做什么

```text
1. 恢复 main.exe 到未改干净状态；
2. 把 main.exe 里 bridge_dsm_mr_start_dsm 的 extName 指针从 start.mr 改成 mrc_loader.ext；
3. 继续注入第 4 参数：
   napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
4. 把原始 jjfb.mrp 复制成 dsm_gm.mrp；
5. 启动 vmrp；
6. 观察是否进入 mrc_loader / robotol / 网络。
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_DIRECT_EXT_BOOT_V26.ps1
```

## 观察

重点看 stdout 里是否出现：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','mrc_loader.ext',...)
mr_c_function_load
robotol
connect
20000 / 21002 / 6009
```

跑完发：

```text
logs\direct_ext_boot_v26_feedback_*.zip
```

以及 vmrp 窗口截图。

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_V26.ps1
```
