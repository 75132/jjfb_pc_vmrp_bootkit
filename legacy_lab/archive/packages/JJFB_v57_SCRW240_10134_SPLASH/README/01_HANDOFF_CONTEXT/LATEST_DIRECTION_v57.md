# 最新路线：v57 SCRW240 + 10134 + splash present

## 已锁定

- v53–v56：handoff、NO FORCE、guest `2FC418`→`ui_mode=0x45`、DispUpEx。
- v57（结合 `docx`）：
  - `SCRW=240`：DrawRect `270→190` axis_remap。
  - `0x10134` size-map 加载原版 loadingbar/bar/textbar。
  - present + refresh（docx：画完必须 refresh）。
  - 无 FORCE mem-write。

## 仍属探针

```text
JJFB_GWY_SPLASH host blit of original assets
host call 0x2FC418
```

## v58 唯一任务

```text
让 guest 2EC6B0 自然 blit 原版 splash（替代 GWY_SPLASH 探针）；
并行追 2DADC4 自然 caller。
```

禁止 FORCE ui_mode / AC8 / progress 作为正式方案。

## 证据

```text
reports/v57_scrw240_splash_implementation.md
reports/v57_scrw240_splash_run_result.md
logs/v57_scrw240_splash_stdout.txt
RUN_V57_SCRW240_10134_SPLASH.ps1
docx/程序编写说明.md
```
