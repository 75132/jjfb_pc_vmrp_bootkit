# 启动 jjfb 程序与资源包 —— 2026-07-29 实测

## 结论

**jjfb 程序与资源包已真正启动并渲染出首帧。** 两条独立验证链全部通过：

1. 产品直驱强成功（`reports/product_direct_jjfb_verdict.md`，run_id `p2_20260729_173336_45649`）
2. Layer-1 首帧门禁 PASS（`out/visual_baseline/gate_20260729_173820/layer1_gate.json`）

## 验证一：产品黄金链（直驱 main.exe）

`RUN_PRODUCT_DIRECT_JJFB.ps1 -SkipBuild -SkipVmrpBuild -Seconds 150` 直接 spawn `out/vmrp_run/main.exe`（Unicorn VM），11 项强门禁全部 `yes`，禁用项全部 `clean`：

| Gate | 结果 |
|------|------|
| DESCRIPTOR_FROZEN | yes |
| TARGET_HASH_VERIFIED | yes（jjfb.mrp sha256 匹配） |
| START_MR_ENTERED | yes |
| MRC_LOADER_RESOLVED_EXACT | yes |
| ROBOTOL_RESOLVED_BY_PROFILE_ALIAS | yes |
| ROBOTOL_BOOTSTRAP_RETURN | yes |
| EXT_VERSION_RETURN_ZERO | yes |
| EXT_APPINFO_RETURN_ZERO | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| ROBOTOL_HANDLER_REGISTERED | yes |
| SCHEDULER_NATURAL_CALLBACK | yes（forced=no，非强制回调） |

禁用项：`gamelist_fast` / `method0_smscfg_write` / `fixed_pc_jump` / `host_fake_ui` / `forced_callback` 全部 `clean`。

`main.exe` sha256：`b784ab50d0d7a5ac043a1c08c006e104b8dd82c45c4b93ece23a1a599cb4303c`

guest 推进深度（vmrp 日志）：`EXT_LOAD` → `BOOTSTRAP_SEQ` → `mrc_loader` 的 `MODULE_ENTER` / `DSM_ENTRY_SELECT` → `BOOT_FAMILY_HANDLER_ENTER a=0x1E209 b=0x9 c=0x30D301`（与 7/28 成功运行一致）→ `JJFB_LIFECYCLE op=ARM period_ms=50`。`resume_mode=direct_lr` 安全模式已确认生效（非不安全的 `callsite`）。

## 验证二：Layer-1 首帧门禁（包装器）

`RUN_JJFB_LAUNCHER.ps1 -SkipBuild -SkipVmrpBuild -HoldSeconds 150` 通过 `JJFB_Launcher.exe` 派生 `main.exe`，等待 guest 绘制首帧并写出 BMP。

`runtime_progress.jsonl` 里程碑序列（pid 10712）：

```
waiting_for_first_frame (post_start_dsm)
pending_bitmap_commit ok      ← 资源包持续加载/提交
drawfp_first_drawn            ← 首帧绘制（loadingbar_or_sprite）
UI_BUILDER_LEAVE_CANDIDATE    ← 离开 UI 构建器
```

`layer1_gate.json` 关键字段：

- `pass = true`，`runtime_pid = 10712`，`runtime_alive = true`
- BMP：`240×320`，230454 字节，sha256 `c789a129…`，`nonBlackPct = 7.08%`，`uniqueColors = 16`，`variance = 1200.44`（非空白，真实首帧）
- **已加载/渲染的资源成员**：`loadingbar!201!29.bmp`、`bar!16!18.bmp`、`textbar!120!30.bmp`、`topleft!15!5.bmp`、`topright!12!4.bmp`
- `drawfp = true`，`first_frame = true`

## 复现命令

```powershell
# 1) 验证黄金链（直驱，无头，捕获 main.exe 结构化追踪）
pwsh -NoProfile -ExecutionPolicy Bypass -File RUN_PRODUCT_DIRECT_JJFB.ps1 -SkipBuild -SkipVmrpBuild -Seconds 150

# 2) 验证首帧渲染（包装器，写 launcher_first_frame.bmp + Layer-1 门禁）
pwsh -NoProfile -ExecutionPolicy Bypass -File RUN_JJFB_LAUNCHER.ps1 -SkipBuild -SkipVmrpBuild -HoldSeconds 150
```

## 关键产物

| 文件 | 说明 |
|------|------|
| `out/visual_baseline/gate_20260729_173820/launcher_first_frame.bmp` | 真实首帧画面（240×320，非空白） |
| `out/visual_baseline/gate_20260729_173820/layer1_gate.json` | Layer-1 门禁结果 |
| `out/visual_baseline/gate_20260729_173820/runtime_progress.jsonl` | 164 条运行里程碑 |
| `reports/product_direct_jjfb_verdict.md` | 产品直驱强成功验收 |
| `logs/product_direct_jjfb_vmrp.txt` | main.exe 完整结构化追踪 |

## 备注

- 工具链（i686 MinGW + Unicorn 1.0.2）本环境可用；`gwy_launcher.exe` / `JJFB_Launcher.exe` / `main.exe` 均为 7/28 23:38 构建产物，未改动 clean core。
- 早期 30s 挂起会卡在 bootstrap（PLAT_10140），guest 约需 50–60s 进入 family handler 并绘制首帧；150s 挂起稳定通过。
- 本次为"真实启动"交付，未引入新的 resume 模式实现（P9 的 epilogue 模式仍待带工具链处落地；当前 `direct_lr` 基线已 product-safe 通过验收）。
