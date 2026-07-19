# JJFB v14：COFF 符号 + direct call hook 版

## v13 结果

v13 失败原因已经明确：

```text
bridge_dsm_mr_start_dsm import not found
```

这不是 jjfb 的问题，而是我前一版判断错了：当前 `main.exe` 里的
`bridge_dsm_mr_start_dsm` 不是 IAT 导入函数，而是静态链接/内部符号。

所以不能用 IAT hook。

## v14 改法

v14 改成解析 `main.exe` 自带的 COFF 符号表：

```text
1. 找符号：_bridge_dsm_mr_start_dsm
2. 算出它在 main.exe 里的真实 VA
3. 扫描所有 E8 direct call
4. 找到 call _bridge_dsm_mr_start_dsm 的位置
5. 在代码空洞写 wrapper
6. 把 direct call 改成 call wrapper
```

wrapper 做的事：

```asm
mov dword ptr [esp+0x10], startParam
jmp _bridge_dsm_mr_start_dsm
```

也就是强制把第 4 个参数从 NULL 改成：

```text
napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

同时 v14 会把 stdout 格式字符串里的 `NULL` 改成 `HOOK`，方便你确认 main.exe 确实被改了。

原来应该显示：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr',NULL): 0x0
```

v14 补丁成功后会显示：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr',HOOK): 0x0
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_DIRECT_CALL_HOOK_AND_BOOT_V14.ps1
```

跑完发：

```text
logs\direct_call_hook_feedback_*.zip
```

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_MAIN_EXE_V14.ps1
```
