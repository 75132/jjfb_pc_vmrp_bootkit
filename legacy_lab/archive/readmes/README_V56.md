# JJFB v56 Upstream Trigger Coverage

这是覆盖到原 `jjfb_pc_vmrp_bootkit` 项目根目录的完整增量包。

运行：

```powershell
.\RUN_V56_UPSTREAM_TRIGGER_COVERAGE.ps1 -Seconds 25
```

本版不会修改 `gwy/jjfb.mrp`，不会 FORCE `ui_mode`，不会启用 AC8/progress，也不会注入 event 5/12。

重点看 `reports/v56_upstream_trigger_run_result.md`。
