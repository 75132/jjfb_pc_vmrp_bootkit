# Cursor 继续开发说明：JJFB / vmrp 当前应修 loader 返回值与 801 init

> 当前文档基于最新交接包 `00_HANDOFF_LATEST.md`。旧的 `AAA-tip.md / tip2` 里关于补 `windows/unicorn...`、装依赖、PATH 的内容，对这台机器已经偏旧。  
> 现在不要回头补依赖，不要重做环境，不要继续盲跳 `start.mr`。

---

## 0. 本机当前状态

本机已经具备编译环境，且 Cursor 侧已经推进到更后面：

```text
windows/unicorn-1.0.2-win32/include/unicorn/unicorn.h：已存在
windows/unicorn-1.0.2-win32/unicorn.lib / unicorn.dll：已存在
windows/SDL2-2.0.10/i686-w64-mingw32：已存在
bin/main.exe：已能编译得到 32-bit 版本，约 476 KB
where gcc / mingw32-make：已指向 C:\msys64\mingw32\bin
```

所以现在不要再让任务停留在：

```text
fatal error: ../windows/unicorn-1.0.2-win32/include/unicorn/unicorn.h
```

那是旧状态。

---

## 1. 当前项目路径

项目根目录：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
```

源码目录：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master
```

运行目录：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102
```

最新交接包核心文档：

```text
00_HANDOFF_LATEST.md
```

关键日志：

```text
evidence\logs\v27_after801_ret0_stdout.txt
evidence\logs\v27_full800_ret0_stdout.txt
evidence\logs\v27_ret0_correct_stdout.txt
evidence\logs\v27_ret0_fixed_stdout.txt
evidence\logs\v27_ret0_fixed2_stdout.txt
evidence\logs\v27_force_success_stdout.txt
```

---

## 2. 当前已打通的链路

已经确认：

```text
start.mr
  ↓
sdk_key line143/147 错误分支已跳过
  ↓
_strCom(601, "mrc_loader.ext")
  ↓
_strCom(800, mrc_loader_buf, 0)
  ↓
mrc_loader.ext 已加载
  ↓
_strCom(801, "", 1)
  ↓
mrc_loader 读 robotol.ext，返回 {ptr,len}
  ↓
第二次 _strCom(800, {ptr,len}, 0)
  ↓
robotol.ext 完整装入
```

关键证据：

```text
mr_get_method(161178)
mr_cacheSync(..., 253504)
ER_RW=@002B1xxx
_mr_c_function_new called
```

所以当前不再是：

```text
_mr_c_load / hsman object
mrc_loader.ext invalid control char
sdk_key
编译依赖
```

---

## 3. 当前真正卡点

现在卡点在 robotol 完整装入之后：

```text
第二次 800 装完 robotol 后，如何合法让 _mr_c_load 返回 0
然后继续执行：
_strCom(801, {1, sysinfo.vmver}, 6)
_gc()
_strCom(801, "", 0)
```

也就是标准 MythroadSDK loader 中的：

```lua
if _mr_c_load() == 0 then
   _strCom(801, {1, sysinfo.vmver}, 6)
   _gc()
   _strCom(801, "", 0)
else
   Exit()
end
```

当前需要推进到 `801 code=6` 和 `801 code=0`，即 robotol init。

---

## 4. 不要再做的事情

不要做：

```text
1. 不要继续补 windows/unicorn 依赖。本机已齐。
2. 不要用 UCRT64 64-bit 混编。生产模拟器用 mingw32/i686。
3. 不要把 mrc_loader.ext 当 bridge_dsm_mr_start_dsm 的 extName。
4. 不要大范围 skip start.mr。
5. 不要手搓 gzip header。
6. 不要写破坏 mr_G_checkcode 的乱指令。
7. 不要继续猜 sdk_key。
8. 不要推倒重来。
```

---

## 5. 当前需要修的核心问题

### 问题 A：第二次 800 的返回值

交接包里写到：

```text
全路径第二次 800 后强制 return 0：有时 ret=0x1，无 After app init
编码“修正”后：曾触发 err:1004 或 mr_exit
```

所以现在要确认：

```text
第二次 _strCom(800, {ptr,len}, 0) 的真实返回值是什么？
它为什么没有让 _mr_c_load 进入成功分支？
能否在 host 侧让 robotol 第二次 800 后返回 0，而不是在 start.mr 里硬改坏字节码？
```

优先建议：

```text
优先在 host / _strCom / TestCom1 里处理返回值；
少改 start.mr 字节码。
```

如果必须 patch `start.mr`，只能跳到同 proto 内已有、合法、已通过校验的 return0 序列。

---

### 问题 B：合法 return 0

不要再手写错误的 LOADK / RETURN。

正确编码规则：

```text
LOADK  A,Bx :  1 | (A << 6) | (Bx << 14)
RETURN A,B  : 27 | (A << 6) | (B << 23)
```

其中：

```text
RETURN A=0, B=2
表示 return R0
```

JMP 正确编码：

```text
encode_jmp(skip) = 20 | (((skip + 131071) & 0x3FFFF) << 6)
```

以前错误写法：

```text
0x00800000 | ((skip & 0xff) << 6) | 20
```

是 off-by-one，会造成假 `_mr_c_load object` 错误。

---

### 问题 C：确认是否走到 801 init

现在要在 host 日志中明确打印：

```text
_strCom code=601
_strCom code=800 第一次
_strCom code=801 code=1
_strCom code=800 第二次
_strCom code=801 code=6
_strCom code=801 code=0
```

如果没有看到：

```text
801 code=6
801 code=0
```

说明还没进入 robotol init。

如果看到 801 code=6/0，但仍无网络，要继续看 robotol 后续 module / timer / event / network。

---

## 6. 具体给 Cursor 的开发任务

### 任务 1：先增加日志，不要先改逻辑

在 `_strCom` / `TestCom1` / 相关 guest-host bridge 处加日志：

```c
printf("[JJFB_801] _strCom code=%d arg1_type=%d arg2=%d\n", code, type, arg2);
printf("[JJFB_800] before load ptr=%p len=%d\n", ptr, len);
printf("[JJFB_800] after load ret=%d mr_c_function_P=%p ER_RW=%p\n", ret, mr_c_function_P, ER_RW);
printf("[JJFB_801] event code=%d ret=%d\n", code, ret);
```

重点是区分：

```text
第一次 800：加载 mrc_loader
801 code=1：mrc_loader 返回 robotol buffer
第二次 800：加载 robotol
801 code=6：平台/version/init 前置事件
801 code=0：mrc_init / robotol init
```

---

### 任务 2：先不要在 start.mr 里硬 return0

如果第二次 800 返回 `1`，先在 host 层定位为什么返回 1：

```text
是 mr_c_function_load 返回 1？
是 wrapper 改成了 1？
是 Lua 层拿到了 boolean / number 不一致？
是 _strCom(800) 的返回值被 TestCom1 转换错？
```

尽量在 host 侧修返回值，让 Lua 原逻辑自然进入成功分支。

---

### 任务 3：若必须 patch start.mr，只做最小控制流 patch

不要生成新的 `LOADK/RETURN`，优先复用同 proto 里已经存在且通过 checkcode 的返回序列。

安全策略：

```text
把第二次 800 后的条件判断 JMP 到已有成功分支
或 JMP 到已有 return0 序列
只改 sBx，不改指令结构，不增删字节
```

每次 patch 后必须验证：

```text
mr_G_checkcode 不报 err:1004
mr_get_method 正常
mr_unzip 正常
```

---

### 任务 4：br_exit 不要直接 exit

为了区分 “Lua Exit()” 和 “模拟器崩溃”，把 `br_exit` 改成记录日志，不要直接 `exit(0)`。

例如：

```c
printf("[JJFB_EXIT] br_exit called, suppressing exit for debug\n");
return 0;
```

至少 debug 阶段先这样。

---

### 任务 5：成功后看端口和模块

成功标准不是白屏变化，而是日志/端口：

```text
出现 801 code=6
出现 801 code=0
出现 robotol init
出现 module.ext / login / network 相关加载
netstat 出现：
20000
21002
6009
211.155.236.*
111.1.17.*
```

---

## 7. 运行命令

在 PowerShell 里：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
.\RUN_V27.ps1
```

或源码目录编译：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master
mingw32-make clean
mingw32-make
```

确认工具链必须是 mingw32/i686：

```powershell
where.exe gcc
where.exe mingw32-make
```

应优先出现：

```text
C:\msys64\mingw32\bin\gcc.exe
C:\msys64\mingw32\bin\mingw32-make.exe
```

---

## 8. 给 Cursor 的一句话总结

当前不要再修编译依赖，也不要再改 sdk_key 或盲跳 `start.mr`。  
**当前任务是：在 robotol.ext 已完整装入之后，修第二次 `_strCom(800)` 的返回值与后续 `_strCom(801, …, 6/0)` 初始化路径，让 `_mr_c_load()` 合法返回 0，并推进到 robotol init / 网络。**
