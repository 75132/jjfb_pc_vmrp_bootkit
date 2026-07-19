# JJFB v27：源码编译 vmrp，测试真正的 C/EXT loader 路线

## 为什么进入 A 路线

v26 已经确认：

```text
mrc_loader.ext 确实能从 jjfb.mrp 里读到；
但 bridge_dsm_mr_start_dsm 直接把 mrc_loader.ext 当脚本入口跑，会报：
mrc_loader.ext:1: invalid control char near char(0)
```

所以 mrc_loader.ext 不能直接作为 extName 启动。  
正确路线仍然是：

```text
start.mr -> _mr_c_load / hsman -> mrc_loader.ext -> robotol.ext -> 网络
```

当前 Windows release 卡在：

```text
start.mr:157: err:call field `hsman' (object)
```

所以现在必须改/编译 vmrp，使 `_mr_c_load / hsman / EXT loader` 路线正常工作。

## v27 做什么

```text
1. 下载 vmrp 源码；
2. patch vmrp.c，让 bridge_dsm_mr_start_dsm 第 4 参数传入 jjfb 的 cfg 参数；
3. 尝试用 mingw32-make 编译；
4. 如果编译成功，把新 main.exe 部署到现有 runtime；
5. 准备 jjfb.mrp：
   - 不再跳过 line157；
   - 只跳过 sdk_key 错误分支 line143/147；
   - 让它正常走 line157 的 _mr_c_load / hsman；
6. 启动测试；
7. 收集 logs/source_build_loader_v27_feedback_*.zip。
```

## 先决条件

你之前的日志显示：

```text
mingw32-make not found
```

所以大概率这次第一次运行会失败在编译环境。  
需要安装 MSYS2/MinGW 32-bit 后再跑。

## MSYS2 安装后，在「MSYS2 MinGW 32-bit」终端运行

```bash
pacman -Syu
pacman -S --needed base-devel mingw-w64-i686-toolchain mingw-w64-i686-SDL2 mingw-w64-i686-unicorn
```

然后把这个目录加到 Windows PATH：

```text
C:\msys64\mingw32\bin
C:\msys64\usr\bin
```

重新打开 PowerShell 再运行 v27。

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SOURCE_BUILD_LOADER_V27.ps1
```

跑完发：

```text
logs\source_build_loader_v27_feedback_*.zip
```

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_V27.ps1
```
