# 最新 GitHub 运行证据索引

仓库：`75132/jjfb_pc_vmrp_bootkit`  
本索引用于把全量静态解析与当前动态进度对齐；报告正文中的结论以 2026-07-21 检索到的 main 分支文件为准。

| 阶段 | 参考文件 | 当前可用结论 |
|---|---|---|
| E9V | `reports/stage_e9v_launcher_visual_parity_verdict.md` | 通用 color-key 与文字 measure/layout 可保留；自然 timer 仍受状态机影响 |
| E9W | `reports/stage_e9w_splash_full_parity_verdict.md` | `@pack`/side-pack 原始像素路径得到验证；AC8/workbuf assist 不得产品化 |
| E9Y | `reports/stage_e9y_natural_splash_state_verdict.md` | workbuf 可经 0x30CBBC→0x30CD82 自然分配；0x2F55FA timer 可到达 |
| E9Y-Fix | `reports/stage_e9y_fix_downimage_event_verdict.md` | 0x2FE82C 真实调用不能形成 ready/AC8 状态，候选被证伪 |
| E9Z | `reports/stage_e9z_gwy_resource_ready_verdict.md` | side-pack registry + event 0x14 不是完整外壳契约 |
| E10A | `reports/stage_e10a_gwy_shell_prelaunch_verdict.md` | direct 可到 splash 但 AC8=0；shell 尚未证明 cfg/update/runapp |
| E10A phase | `reports/e10a_shell_phase_trace.csv` | 已记录 gbrwcore start/continue → gamelist load |
| E10A VFS | `reports/e10a_shell_vfs_trace.csv` | file call context 仍为零值占位，需在真实 API 边界接上下文 |

注意：当前 E10A verdict 文件存在里程碑与结论不一致，必须由 `run_id + milestone gate` 修复后再用于 shell-vs-direct 成功判断。
