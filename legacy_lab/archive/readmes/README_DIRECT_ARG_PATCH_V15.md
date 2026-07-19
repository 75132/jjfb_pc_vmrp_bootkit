# JJFB v15：直接改 callsite 的第 4 参数，不再 wrapper hook

## v14 结果

v14 的静态补丁确实写进去了：

```text
selected_call=0x225f
wrapper_va=0x40d6e8
param=napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

但是运行结果没有推进：

```text
stdout 只有 9 条 libpng warning
netstat 没有 20000 / 21002
```

这说明 v14 的 wrapper hook 可能让 main.exe 卡住了，或者 stdout 缓冲没有吐出。为了减少干扰，v15 不再写 wrapper。

## v15 改法

v14 已经找到了真实 callsite：

```text
call bridge_dsm_mr_start_dsm at file offset 0x225f
```

在它前面能看到原始参数设置：

```asm
c7 44 24 0c 00 00 00 00
```

这就是：

```asm
mov dword ptr [esp+0x0c], 0
```

也就是第 4 参数 NULL。

v15 直接把这个 0 改成 startParam 字符串地址：

```asm
mov dword ptr [esp+0x0c], startParam
```

不改 call，不写 wrapper，不跳转，风险更小。

同时把日志里的 `NULL` 改成 `ARGP`，方便确认补丁版 main.exe 是否运行：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr',ARGP): 0x0
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_DIRECT_ARG_PATCH_AND_BOOT_V15.ps1
```

跑完发：

```text
logs\direct_arg_patch_feedback_*.zip
```

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_MAIN_EXE_V15.ps1
```
