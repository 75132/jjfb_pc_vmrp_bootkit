# JJFB v13：IAT hook 二进制补丁版

## v12 结果

v12 没有真的改成功 main.exe：

```text
RuntimeError: no safe patchable NULL argument pattern found
```

后面的 boot 只是继续启动了未补丁版 main.exe，所以 stdout 仍然只有：

```text
9 条 libpng warning
```

## v13 改法

v12 是想直接找 `NULL` 参数位置，失败了。

v13 换成更稳的方式：hook `bridge_dsm_mr_start_dsm` 的调用。

原理：

```text
原 main.exe：
call [bridge_dsm_mr_start_dsm]
第四参数 = NULL

v13：
1. 找 main.exe 的 import table，定位 _bridge_dsm_mr_start_dsm 的 IAT 地址
2. 找所有 call [IAT] 的调用点
3. 找一个代码空洞
4. 写入 wrapper：
   mov dword ptr [esp+0x10], startParam
   jmp dword ptr [IAT]
5. 把 call [IAT] 改成 call wrapper
```

这样不需要在原地扩展 `push 0`，只是在调用前把第 4 个参数改成：

```text
napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_IAT_HOOK_AND_BOOT_V13.ps1
```

跑完发：

```text
logs\iat_hook_feedback_*.zip
```

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_MAIN_EXE_V13.ps1
```

## 成功标志

stdout 里应该不再是：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr',NULL)
```

而应该能继续推进，或者至少出现新行为。
