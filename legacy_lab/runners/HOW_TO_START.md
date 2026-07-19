# 怎么启动 JJFB（冒泡网游 / GWY Launcher）

一句话：在项目根目录用 PowerShell 跑 `RUN_JJFB.ps1`。

---

## 1. 环境要求

| 依赖 | 说明 |
|------|------|
| Windows 10+ | 当前脚本按 Win32 路径写的 |
| PowerShell | 在项目根目录执行 |
| MSYS2 MinGW32 | `C:\msys64\mingw32\bin` 里要有 `gcc`（32 位） |
| Python 3 | 命令行能跑 `python` 或 `py` |
| 游戏资源 | `game_files\mythroad\320x480\gwy\jjfb.mrp` 必须存在 |

可选检查：

```powershell
gcc --version
python --version
Test-Path .\game_files\mythroad\320x480\gwy\jjfb.mrp
```

---

## 2. 推荐启动（日常）

打开 PowerShell，进入项目根目录：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
.\RUN_JJFB.ps1
```

这会：

1. 把 `game_files\mythroad\320x480` 同步到 runtime
2. 校验原始 `jjfb.mrp` 未被改动
3. 生成 `sdk_key.dat`
4. 编译 32 位 `main.exe`
5. 以 **GWY Launcher Mode** 启动（窗口保持打开）
6. 你关掉窗口后，脚本再汇总日志到 `logs\jjfb_gwy_clean_stdout.txt`

窗口会弹出来（SDL），**不会自动关闭**；自己关窗口（或在 PowerShell 里 Ctrl+C）即可。

---

## 3. 常用参数

```powershell
# 已编译过，跳过重新编译（更快）
.\RUN_JJFB.ps1 -SkipBuild

# 资源已同步过，跳过 robocopy
.\RUN_JJFB.ps1 -SkipBuild -SkipResourceCopy

# 组合：快速复现
.\RUN_JJFB.ps1 -SkipBuild -SkipResourceCopy
```

`RUN_JJFB.ps1` 只是入口别名，实际实现是根目录的 `RUN_V71_PRESENT_COALESCE.ps1`。两个脚本参数相同。

---

## 4. 启动后看什么

### 成功标志（日志里应有）

```text
[JJFB_GWY_LAUNCH] cfg_index=36 ...
[JJFB_GWY_ROOT] mythroad_root=...\mythroad\320x480
[JJFB_LOADER] ...
[JJFB_ROBOTOL] ...
```

主日志：

```text
logs\jjfb_gwy_clean_stdout.txt
```

运行时原始输出（脚本也会汇总到上面）：

```text
runtime\vmrp_win32\vmrp_win32_20220102\jjfb_loader_stdout.txt
```

### 需要警惕

```text
[JJFB_FILEOPEN_MISS]   → 资源根 / 路径映射不对
BMP_LOAD ... FAIL      → 资源文件缺失或路径错
```

大量 `FILEOPEN_MISS` 时，先查 `game_files\mythroad\320x480` 是否完整、是否拷进了 runtime。

---

## 5. 这次启动在干什么（概念）

不是重做游戏 UI，而是仿真冒泡网游外壳的启动链：

```text
JJFB_GWY_LAUNCHER_MODE=1
→ 绕过 gwy 外壳强制更新
→ cfg index=36
→ startGame/runapp 等价参数
→ 加载原始 gwy/jjfb.mrp
→ mrc_loader.ext / robotol.ext 自然接管
```

启动参数契约：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

显示约定：

```text
guest LCD = 240x320
SDL 窗口 = 320x480（拉伸显示）
资源根   = mythroad/320x480
```

默认关闭会让画面显得“假模拟”的 host 行为（需要时可手动打开）：

```text
JJFB_FRAME_CLEAR=1   # host 每 tick 清黑屏（默认关）
```

底部木纹条重复时保持开启（平台布局校正，不是假 UI）：

```text
JJFB_Y828_ZERO=1     # 默认开：去掉 +40 下移导致的多出一条
JJFB_TEXTBAR_DEDUP=1 # 默认开：同 tick 重叠 textbar 去重
```

临时探针（**非正式方案**，默认关闭；仅诊断时手动打开）：

```text
$env:JJFB_SKIP_NET_LOGIN="1"
$env:JJFB_SKIP_NET_LOGIN_MS="2000"
# splash ~2s 后种子 AC8/B6C/134D 成功门（不是正式启动链）
```

正式启动不要设置上述变量。

---

## 6. 目录对应关系

| 角色 | 路径 |
|------|------|
| 源资源（不要改游戏逻辑） | `game_files\mythroad\320x480\` |
| 运行时拷贝 | `runtime\vmrp_win32\vmrp_win32_20220102\mythroad\320x480\` |
| 源码 / 编译 | `runtime\vmrp_src_build_v27\vmrp-master\` |
| 可执行文件 | `runtime\vmrp_win32\vmrp_win32_20220102\main.exe` |
| 主入口脚本 | `RUN_JJFB.ps1` |

目标 MRP：

```text
game_files\mythroad\320x480\gwy\jjfb.mrp
```

---

## 7. 常见问题

### `gcc` 找不到 / 编译失败

确认 MSYS2 32 位工具链：

```powershell
Test-Path C:\msys64\mingw32\bin\gcc.exe
$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
gcc -m32 -v
```

### `Python 3 is required`

安装 Python 3，或确保 `python` / `py` 在 PATH 里。

### `canonical source target missing`

缺少：

```text
game_files\mythroad\320x480\gwy\jjfb.mrp
```

把完整 mythroad/320x480 资源树放进 `game_files`。

### `clean JJFB overlay missing...`

源码补丁不完整或被改乱了。需要保证 `bridge.c` / `main.c` / `vmrp.h` 等仍含当前 clean baseline 标记；不要用旧的 v50 以前 UI 探针脚本当日常入口。

### 不要用这些当日常启动

```text
scripts\runners\RUN_V43_*.ps1   # splash/UI 实验
scripts\runners\RUN_V46_*.ps1
scripts\runners\RUN_V47_*.ps1
archive\runners\...             # 历史归档
```

日常只用根目录：

```text
.\RUN_JJFB.ps1
```

---

## 8. 最短复现命令

第一次（完整）：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
.\RUN_JJFB.ps1
```

之后（快）：

```powershell
.\RUN_JJFB.ps1 -SkipBuild -SkipResourceCopy
```

看日志（关掉窗口之后）：

```powershell
Select-String -Path .\logs\jjfb_gwy_clean_stdout.txt -Pattern "JJFB_GWY_LAUNCH|JJFB_LOADER|JJFB_ROBOTOL|FILEOPEN_MISS"
```
