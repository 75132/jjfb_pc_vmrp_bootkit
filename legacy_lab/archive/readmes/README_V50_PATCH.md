# JJFB v50 GWY Launcher Shim 增量包

本包基于 `jjfb_pc_vmrp_bootkit(1).zip` 的最新文档路线制作。

## 使用方式

把本包内容解压并覆盖到原项目根目录，然后在 Windows PowerShell 运行：

```powershell
.\RUN_V50_GWY_LAUNCHER_MODE.ps1 -Seconds 25
```

不要运行 `RUN_V49_R4_GATE.ps1` 作为主线。

## 本包做了什么

- `JJFB_GWY_LAUNCHER_MODE=1` 时直接使用 canonical `gwy/jjfb.mrp`；
- 完整复制并保持 `mythroad/240x320/gwy` 资源树；
- 添加 guest → host 路径映射和 `FILEOPEN/MISS` 证据；
- 修正 code=8 appInfo：APPID/APPVER 从 MRP header 读取（400101/12），不再误用 nextid=482；
- 清除 v49 UI/state force 环境变量；
- 自动生成 v50 运行结果报告。

## 运行后需要带回的文件

```text
logs/v50_gwy_launcher_mode_stdout.txt
logs/v50_gwy_launcher_mode_stderr.txt
reports/v50_gwy_launcher_run_result.md
```

下一轮只根据真实首个 `FILEOPEN_MISS` 或 loader 返回值继续，不再根据 UI 动画猜状态。
