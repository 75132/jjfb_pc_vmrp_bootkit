# JJFB v57 Lifecycle Source Coverage

解压到 `jjfb_pc_vmrp_bootkit` 项目根目录并覆盖同名文件，然后运行：

```powershell
.\RUN_V57_LIFECYCLE_SOURCE_COVERAGE.ps1 -Seconds 25
```

只看两个输出：

- `reports57_lifecycle_source_run_result.md`
- `logs57_lifecycle_source_stdout.txt`

本版不注入 C0、method1/5、command10002，不 FORCE ui_mode，不画 host UI。
