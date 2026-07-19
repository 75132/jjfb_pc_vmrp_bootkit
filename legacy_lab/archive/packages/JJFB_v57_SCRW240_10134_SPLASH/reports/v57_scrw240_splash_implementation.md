# v57 SCRW240 / 10134 / splash 实施报告

## 1. 本轮目标

结合 `docx/程序编写说明.md`（`SCRW`/`SCRH`、绘图后必须 refresh）与 v56 blocker（DrawRect `x=270`、10134 no-match）：

```text
修复平台图形契约，让原版 splash 资源进 240×320 画布并 refresh
不做 host FORCE ui_mode / AC8 / progress driver
```

## 2. 修改文件

| 文件 | 变更 |
|------|------|
| `bridge.c` | `jjfb_bmp_req_from_bytes`（10134 size→name）；DrawRect `axis_remap` 320→240；`jjfb_gwy_present_splash_assets` |
| `RUN_V57_SCRW240_10134_SPLASH.ps1` | 运行脚本 |
| `scripts/v57_analyze_scrw240_splash_log.py` | 日志分析 |
| `reports/v57_*.md` / `LATEST_DIRECTION_v57.md` | 报告 |

## 3. 环境变量

| 变量 | 作用 |
|------|------|
| `JJFB_GWY_LAUNCHER_MODE=1` | GWY shim |
| `JJFB_FORCE_UI_MODE=0` | 禁止 FORCE |
| `JJFB_GWY_BRINGUP=1` | dims/C44/2FC418 |
| `JJFB_GWY_SPLASH_BLIT=1` | present 原版 splash（`0` 关闭） |
| `JJFB_AXIS_FIX=1` | getter 轴修复 |

## 4. 运行命令

```powershell
.\RUN_V57_SCRW240_10134_SPLASH.ps1 -Seconds 25
```

## 5. 关键日志

```text
[JJFB_10134] size-map bytes=0x2D8A -> loadingbar!201!29.bmp
[JJFB_BMP_LOAD] cache .../loadingbar!201!29.bmp.rgb565 -> 11658 bytes
[JJFB_10134_BMP] #1 ... loaded=1
[JJFB_10134_BMP] #2 bar!16!18.bmp loaded=1
[JJFB_10134_BMP] #3 textbar!120!30.bmp loaded=1
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_BLIT_KEYED] loadingbar ... x=19 y=220 ... tag=GWY_SPLASH
[JJFB_GWY_SPLASH] presented 3 original assets (refresh after draw)
```

## 6. 结论

1. **docx SCRW=240**：`x=270` 来自按宽度 320 右对齐；remap 后为屏内 `190`。
2. **10134**：splash 尺寸请求无 2d92dc 名时，size-map 可加载 `jjfb_bmp_cache` 原版 RGB565。
3. **refresh**：present 后 `guiDrawBitmap`（对齐 docx「画完必须 refresh」）。
4. GWY_SPLASH blit 仍是 **探针**（补 2EC6B0 未自然走到的 present）；资源本身是原版。

## 7. 被证伪

- 「只修 DispUpEx / ui_mode=0x45 即可出 splash」——否；10134 无像素时仍空白。
- 「x=270 是 ERW+0x830 应改成 240」——否；v38 锁定 `0x830=320`；应对齐 DrawRect 布局轴。

## 8. 当前 blocker

```text
natural 2EC6B0 blit path still unused (host GWY_SPLASH present is probe)
slogo (0x4714) not requested this run
2DADC4 natural caller still unknown
```

## 9. 下一步

1. 让 guest `2EC6B0` 自然 blit（替换 GWY_SPLASH 探针）。
2. 并行：`2DADC4` 自然 caller。
3. 仍禁止 FORCE ui_mode / AC8 / progress 作为正式方案。
