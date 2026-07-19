# Cursor 开发说明：推进 JJFB / vmrp 的 C/EXT loader 支持

> 目标：不要再盲目 patch `jjfb.mrp/start.mr`。当前要让 **PC 版 vmrp 正确支持 jjfb 的 `_mr_c_load / hsman / C/EXT loader` 路线**，使 `start.mr -> mrc_loader.ext -> robotol.ext -> 网络` 能继续执行。

---

## 0. 当前编译环境已经可用

Windows PowerShell 中已验证：

```powershell
where.exe gcc
# C:\msys64\ucrt64\bin\gcc.exe

where.exe mingw32-make
# C:\msys64\ucrt64\bin\mingw32-make.exe

where.exe pkg-config
# C:\msys64\ucrt64\bin\pkg-config.exe

where.exe sdl2-config
# C:\msys64\ucrt64\bin\sdl2-config

gcc --version
# gcc.exe (Rev5, Built by MSYS2 project) 16.1.0

mingw32-make --version
# GNU Make 4.4.1

pkg-config --modversion sdl2
# 2.32.10

pkg-config --modversion unicorn
# 2.1.4
```

PowerShell 中不要用 `where gcc` 判断，要用：

```powershell
where.exe gcc
where.exe mingw32-make
where.exe pkg-config
where.exe sdl2-config
```

---

## 1. 项目路径

主项目路径：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
```

vmrp 源码路径：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master
```

runtime 路径：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102
```

重要日志路径：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs
```

重点查看这些日志：

```text
logs\source_build_loader_v27_report.txt
logs\source_build_loader_v27_env_check.txt
logs\vmrp_v27_patched.c
logs\vmrp_source_audit_report.txt
logs\vmrp_source_search_hits.json
logs\start_mr_audit_report.json
logs\start_mr_consts.json
logs\start_mr_pc90_150.json
logs\loader_binary_scan.txt
```

---

## 2. 先跑一次 v27，获得真实 build 错误

现在编译环境已经就绪，先不要手动乱改源码，先在 PowerShell 里运行：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SOURCE_BUILD_LOADER_V27.ps1
```

如果编译失败，先看：

```text
logs\source_build_loader_v27_report.txt
```

里面会有 `mingw32-make` 的真实编译错误。

---

## 3. 已经确认的事实

### 3.1 `jjfb.mrp` 可以被 PC vmrp 启动

已经验证：

```text
dsm_gm.mrp <= jjfb.mrp
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr',...): 0x0
```

说明入口不是完全错的。

### 3.2 main.exe 第 4 参数必须注入

原 release 的 `main.exe` 调用类似：

```c
bridge_dsm_mr_start_dsm(..., "dsm_gm.mrp", "start.mr", NULL)
```

需要改成传入：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

v27 已经尝试 patch `vmrp.c`，请检查 `logs\vmrp_v27_patched.c` 里的改动是否正确。

### 3.3 `start.mr` 的 sdk_key 错误分支可以跳过

原始错误：

```text
start.mr:143
cann`t find sdk key!
```

我们已经通过 patch `start.mr` 字节码跳过 line143 / line147 的错误分支，能进入 line157。

这个 patch 可以保留，但不要继续大范围跳行。

### 3.4 当前核心卡点是 C/EXT loader

跳过 sdk 错误后，错误变成：

```text
start.mr:157: err:call global `_mr_c_load' (object)
```

或者：

```text
start.mr:157: err:call field `hsman' (object)
```

这说明：

```text
start.mr 已经进入正常流程；
但 PC vmrp 当前没有正确支持 _mr_c_load / hsman / C 扩展加载链。
```

### 3.5 `mrc_loader.ext` 不能直接当脚本启动

已经验证把入口改成：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','mrc_loader.ext',...)
```

会报：

```text
mrc_loader.ext:1: invalid control char near `char(0)'
```

说明 `mrc_loader.ext` 是二进制 C/EXT 模块，不是 `start.mr` 这种脚本/字节码入口。不要再走“直接 extName=mrc_loader.ext”的路线。

---

## 4. Cursor 的开发目标

修复 vmrp 源码，使 `start.mr` 能正常调用：

```text
_mr_c_load / hsman
```

并加载：

```text
mrc_loader.ext
robotol.ext
后续 module.ext
```

最终尽量推进到：

```text
网络连接 20000 / 21002 / 6009
```

---

## 5. Cursor 不要做的事

不要继续做这些：

```text
1. 不要继续猜 sdk_key.dat。
2. 不要继续把 mrc_loader.ext 当 bridge_dsm_mr_start_dsm 的 extName 直接启动。
3. 不要继续 patch start.mr 大范围跳行。
4. 不要把 _error 随便替换成 _gc / _strCom / _mr_c_buf。
5. 不要只看界面白屏，要看 stdout 和 logs。
```

当前不是游戏文件没启动，而是 **vmrp 的 C/EXT loader 支持不完整**。

---

## 6. 优先查看的源码文件

在源码目录：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master
```

优先查看：

```text
vmrp.c
bridge.c
main.c
mythroad/
mrc/
```

全文搜索：

```text
_mr_c_load
_mr_c_buf
hsman
_strCom
_com
mr_c_function
mr_c_function_load
_mr_c_function_new
mr_load
mrc_loader
robotol
bridge_dsm_mr_start_dsm
mr_get_method
```

---

## 7. 建议开发顺序

### 第一步：先让 vmrp 源码能编译

进入源码目录：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master
mingw32-make clean
mingw32-make
```

如果失败，先修 Makefile / include / library 路径问题。

当前环境是 UCRT64：

```text
C:\msys64\ucrt64\bin
```

可以用：

```powershell
pkg-config --cflags sdl2
pkg-config --libs sdl2
pkg-config --cflags unicorn
pkg-config --libs unicorn
```

如果 Makefile 里写死了旧路径或旧库名，改成 `pkg-config` 获取 SDL2 和 unicorn。

建议 Makefile 中使用类似：

```makefile
CFLAGS += $(shell pkg-config --cflags sdl2 unicorn)
LDFLAGS += $(shell pkg-config --libs sdl2 unicorn)
```

---

### 第二步：确认 v27 的 `vmrp.c` 参数补丁

检查 `vmrp.c` 中调用：

```c
bridge_dsm_mr_start_dsm(...)
```

确保第四参数不是 `NULL`，而是：

```c
char *startParam = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
```

调用应类似：

```c
bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);
```

不要改成 `mrc_loader.ext`，`extName` 保持：

```c
"start.mr"
```

---

### 第三步：只保留 sdk 错误分支跳过

v27 会生成：

```text
logs\jjfb_skip_sdk_only_v27.mrp
```

这个版本只跳过：

```text
line143
line147
```

不要跳过 line157，因为 line157 就是我们要修的 loader 入口。

---

### 第四步：给 loader 加日志

在 vmrp 源码里增加日志，定位 `_mr_c_load / hsman` 到底是什么类型、在哪里注册、为什么作为 object 被调用。

建议日志前缀统一：

```text
[JJFB_LOADER]
```

建议加这些日志：

```c
printf("[JJFB_LOADER] bridge_dsm_mr_start_dsm filename=%s extName=%s entry=%s\n", filename, extName, entry);
printf("[JJFB_LOADER] _mr_c_load lookup type=%d\n", ...);
printf("[JJFB_LOADER] hsman lookup type=%d\n", ...);
printf("[JJFB_LOADER] _strCom code=%d arg=%s\n", code, arg);
printf("[JJFB_LOADER] _com code=%d\n", code);
printf("[JJFB_LOADER] trying load ext=%s\n", extName);
printf("[JJFB_LOADER] ext offset=%u size=%u\n", off, size);
printf("[JJFB_LOADER] mr_c_function_load addr=%p\n", ...);
printf("[JJFB_LOADER] mr_c_function_load ret=%d\n", ret);
printf("[JJFB_LOADER] _mr_c_function_new called helper=%p\n", ...);
```

---

## 8. 需要重点理解的调用链

目标调用链应是：

```text
bridge_dsm_mr_start_dsm("dsm_gm.mrp", "start.mr", param)
    ↓
start.mr 解压执行
    ↓
读取 sdk_key.dat
    ↓
跳过 sdk 错误分支
    ↓
line157 调用 _mr_c_load / hsman
    ↓
加载 jjfb.mrp 内部 mrc_loader.ext
    ↓
mrc_loader.ext 初始化
    ↓
_mr_c_function_new 回调
    ↓
加载 robotol.ext / module.ext
    ↓
开始网络连接
```

现在卡在：

```text
line157 调用 _mr_c_load / hsman
```

---

## 9. 对 `_mr_c_load / hsman` 的具体修复方向

当前报：

```text
call global `_mr_c_load' (object)
call field `hsman' (object)
```

说明 Lua/MRP 层看到的是一个 object，但它没有被正确作为 callable function 或 callable method 使用。

需要检查：

```text
1. `_mr_c_load` 是在哪里注册到全局表的？
2. 它为什么是 object？
3. 这个 object 是否应该有 __call / method table / hsman 字段？
4. `hsman` 字段是否应该是 C 函数？
5. `_strCom(601, ...)` / `_strCom(800, ...)` 是否是实际加载 C function 的路径？
```

不要直接把 object 改成普通字符串或空函数。要让它按原平台的 EXT loader 语义工作。

---

## 10. 参考已有结果

### v22

成功跳过 sdk 错误分支，进入 line157：

```text
start.mr:157: err:call global `_mr_c_load' (object)
```

### v23

跳过 line157 后变成：

```text
start.mr:157: err:call field `hsman' (object)
```

说明 line157 附近就是 `_mr_c_load / hsman` loader 逻辑。

### v24

跳过后续多个 line，没有任何推进：

```text
pc113_line163
pc122_line165
pc124_line166
pc129_line168
pc132_line169
pc136_line171
pc147_line172
pc148_line174
pc150_line175
```

全部没有：

```text
mrc_loader
robotol
connect
20000 / 21002 / 6009
```

说明不能继续靠跳行。

### v26

直接启动：

```text
mrc_loader.ext
```

报：

```text
mrc_loader.ext:1: invalid control char near `char(0)'
```

说明 `mrc_loader.ext` 是 C/EXT 二进制模块，必须通过 C/EXT loader 进入。

---

## 11. 编译成功后的测试命令

如果 Cursor 成功编译出新的 `main.exe`，先复制到：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\main.exe
```

然后回到 bootkit 根目录运行：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SOURCE_BUILD_LOADER_V27.ps1
```

或手动启动：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102
.\main.exe
```

---

## 12. 成功标准

至少要达到：

```text
不再出现：
cann`t find sdk key
call global `_mr_c_load' (object)
call field `hsman' (object)
mrc_loader.ext: invalid control char
```

更好的推进标志：

```text
stdout 出现 mrc_loader.ext 被加载
stdout 出现 _mr_c_function_new called
stdout 出现 robotol.ext / bigworldmapmodule.ext / mainmenumodule.ext
netstat 出现：
20000
21002
6009
211.155.236.*
111.1.17.*
```

---

## 13. 给 Cursor 的一句话总结

**不要再改 jjfb.mrp 逻辑了。当前核心任务是修 vmrp 的 `_mr_c_load / hsman / C-EXT loader`，让 start.mr 正常加载 mrc_loader.ext，而不是把 ext 当脚本跑。**
