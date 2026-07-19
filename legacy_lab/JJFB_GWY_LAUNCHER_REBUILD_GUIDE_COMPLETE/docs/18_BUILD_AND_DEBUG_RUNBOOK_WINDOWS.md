# Windows / Cursor 构建与调试运行手册

## 1. 推荐环境

- Windows 10/11 x64；
- MSYS2 MinGW32 或明确的 i686-w64-mingw32 toolchain；
- CMake + Ninja（或保留 upstream Makefile 作为 baseline）；
- Python 3.11+ 仅用于工具和测试；
- SDL2 与 Unicorn 使用固定 32-bit build；
- Cursor 打开 clean repo，不直接打开旧 bootkit 当主工程。

## 2. 目录变量

不要在源码中写用户绝对路径。建议：

```powershell
$env:GWY_RESOURCE_ROOT = 'D:\Games\mythroad\320x480'
$env:GWY_PROFILE       = "$PWD\profiles\jjfb.json"
$env:GWY_USER_DATA     = "$PWD\out\userdata"
```

profile 可引用 `${GWY_RESOURCE_ROOT}`，由 launcher 展开并记录最终值。

## 3. 32 位构建预检

```powershell
cmake -S . -B build-i686 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-i686.cmake `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-i686
```

验证：

```powershell
objdump -f .\build-i686\gwy_launcher.exe
```

必须是 i386/PE32；不要误用 64-bit DLL。

## 4. 每次运行前的 preflight

```powershell
python .\tools\audit_launcher_core.py .
python .\tools\mrp_inspect.py "$env:GWY_RESOURCE_ROOT\gwy\jjfb.mrp" --json out\jjfb_mrp.json
python .\tools\gwy_cfg_inspect.py "$env:GWY_RESOURCE_ROOT\gwy\cfg.bin" --index 36 --json out\cfg36.json
ctest --test-dir build-i686 --output-on-failure
```

启动器本身应再做一次 hash/profile 校验，不能只依赖脚本。

## 5. 推荐命令模式

```powershell
.\build-i686\gwy_launcher.exe inspect --profile .\profiles\jjfb.json
.\build-i686\gwy_launcher.exe validate --profile .\profiles\jjfb.json
.\build-i686\gwy_launcher.exe launch --profile .\profiles\jjfb.json --trace out\run.jsonl
.\build-i686\gwy_launcher.exe replay --trace tests\fixtures\scheduler_trace.jsonl
```

## 6. 分层 debug 开关

允许：

```text
--trace-subsystem vfs
--trace-subsystem ext
--trace-subsystem platform
--trace-subsystem scheduler
--max-runtime-seconds 25
--offline
```

禁止重新出现：

```text
FORCE_UI_MODE
FAMILY_C0_AFTER_...
PATH_A_EVENT...
PROGRESS_DRIVER
```

## 7. 崩溃收集

每次运行目录保存：

```text
launch_manifest.json
trace.jsonl
summary.md
stdout.txt
stderr.txt
crash_context.json（若失败）
```

`crash_context` 至少含：

- build/profile/target hashes；
- state machine state；
- current EXT/member；
- guest PC/LR/SP；
- call depth；
- last 32 platform calls；
- last 32 VFS operations；
- pending scheduler queue；
- 不自动 dump 用户隐私/凭据。

## 8. 问题定位顺序

```text
validate 失败 → profile/cfg/hash
FILEOPEN_MISS → VFS
member miss → archive/resolver
解压失败 → MRP decoder
helper 未注册 → EXT mapping/ABI
init 非 0 → platform version/appInfo/init contract
注册存在但不运行 → scheduler
画面错误 → display/present/input
连接错误 → network/远端服务（独立于 launcher）
```

任何阶段都不要跳回固定游戏地址写状态。
