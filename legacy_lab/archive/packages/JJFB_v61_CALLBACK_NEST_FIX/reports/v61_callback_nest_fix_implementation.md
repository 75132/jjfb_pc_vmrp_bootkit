# v61 Callback Nest-Fix 实施报告

## 1. goal

对照 `JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE` + v60 结果：

- v60 已：`family app=2` → timer → `mrc_resume` → `entry_2F5404 ×1`
- 但首次 `ext_call code=2` **无 ret**，卡在 `family app=7` 嵌套 emu

本轮修复平台契约：helper/timer 执行期间 defer `1E209`，返回后再 flush。

## 2. root cause

```text
ext_call(code=2) runCode(helper)
  → mrc_timerTimeout → 3056C6 → entry_2F5404
  → sendAppEvent(0x1E209, app=7)
  → jjfb_in_guest_handler==0 → 立刻 flush → 嵌套 uc_emu_start
  → 挂死（25s 杀进程；无 code=2 ret）
```

`jjfb_run_guest_thumb` / `flush` 原本会设 `jjfb_in_guest_handler`，但 **`bridge_dsm_ext_call` 没有**。

## 3. modified files

| 文件 | 作用 |
|------|------|
| `bridge.c` | ext_call 期间 `in_guest=1`；defer `1E209`；返回后 flush |
| `RUN_V61_CALLBACK_NEST_FIX.ps1` | 跑测 |
| `scripts/v61_analyze_callback_nest_fix_log.py` | 分析 |

## 4. new env vars

无（沿用 v60：`JJFB_FAMILY_APP2_AFTER_INIT` / `JJFB_MRC_RESUME_AFTER_INIT`）。

## 5. run command

```powershell
.\RUN_V61_CALLBACK_NEST_FIX.ps1 -Seconds 25 -SkipResourceCopy
```

## 6. key logs

```text
[JJFB_V61_NEST] defer 1E209 app=7 ... (avoid nested emu during guest)
[JJFB_V56_CALLBACK] tail_call_305EB8 #N
[JJFB_V56_PERIODIC] entry_305EB8 #N ... B70=0 ...
[JJFB_801] ext_call code=2 ret=0
[JJFB_V61_NEST] flush deferred 1E200 after ext_call code=2
entry_2F5404 多次（持续）
```

| 探针 | v60 | v61 |
|------|----:|----:|
| entry_2F5404 | 1 | **多次** |
| code=2 ret | 0 | **有** |
| tail_call_305EB8 | 0 | **多次** |
| entry_305EB8 | 0 | **多次** |
| B70 | — | **恒 0** |
| gate / writer / C0 | 0 | 0 |

## 7. conclusion

嵌套挂死已消除；Path C 回调链已持续跑到 `305EB8`。

当前 blocker：`305EB8` 见 `B70=0`，未进 `305EF4`/`2DADC4`/writer。

## 8. disproved assumptions

- ~~只注册 `2F5405` 不够调度~~：v60 已证可 entry；v61 证可持续。
- ~~缺 C0 才导致不进 305EB8~~：不注入 C0 也能到 305EB8。

## 9. next minimal task

v62：弄清谁写 `ERW+B70`、`305EB8`→`305EF4` 条件；仍禁止 FORCE ui_mode / 注入 C0 / host 画 UI。

备注：pending `1E209` 目前单槽，同 tick 内 `app=7` 可能被 `app=5` 覆盖——若后续 UI/ops 异常再改队列。
