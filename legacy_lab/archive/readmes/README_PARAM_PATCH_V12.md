# JJFB v12：给 PC vmrp 注入第三参数，两条路：二进制补丁 / 源码编译

## v11 结果结论

v11 已经确认：

```text
1. vmrp.c 源码补丁成功生成；
2. 当前电脑没有 mingw32-make，所以没有编译出 patched main.exe；
3. 现有 main.exe 里确实硬编码：
   bridge_dsm_mr_start_dsm('%s','%s',NULL)
4. 参数字符串还没有进 main.exe。
```

另外，v11 生成的 patched vmrp.c 里 `printf` 的 `\n` 被写成了真实换行，源码编译会报错。v12 已修复。

## 当前最接近成功路线

不是继续改 MRP 文件，而是让 vmrp 的第三参数从 NULL 变成：

```text
napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

v12 提供两条路：

## 路线 A：二进制自动补丁，先试这个

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BINARY_PATCH_AND_BOOT_V12.ps1
```

它会：

```text
1. 备份 runtime/.../main.exe
2. 在 main.exe 里找 dsm_gm.mrp/start.mr/NULL 调用点
3. 找空洞写入 startParam 字符串
4. 尝试把第四参数 NULL 指针改成 startParam 指针
5. 如果补丁成功，直接用 jjfb + sdk_key=g:u2 启动测试
6. 打包 binary_patch_feedback_*.zip
```

如果它提示 `push 0 short form cannot be patched safely`，说明这个 release 的 main.exe 需要源码编译或更复杂跳板补丁。

## 路线 B：源码编译

先安装 MSYS2/MinGW 32-bit 环境，然后运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SOURCE_BUILD_PATCHED_VMRP_V12.ps1
```

它会：

```text
1. 修复并 patch vmrp.c
2. 尝试用 mingw32-make 编译
3. 编译成功则把 patched main.exe 复制到 runtime/vmrp_win32/.../main.exe
4. 再启动 jjfb 测试
```

## 回传

无论哪条路，跑完发：

```text
logs\binary_patch_feedback_*.zip
```

或者：

```text
logs\source_build_feedback_*.zip
```
