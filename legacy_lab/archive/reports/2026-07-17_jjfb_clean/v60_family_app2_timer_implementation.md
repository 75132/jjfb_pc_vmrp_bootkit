# v60 Family app=2 Timer + Resume 实施报告

## 1. goal

对照 `JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE`：

- 注册生产者：EXT method1+cmd10002 **或** method5（resume）。
- v59 只做了 resume → 出现 `timer err:1000`（`mrc_timerStart(t=NULL)`）。

本轮补齐 **timer 对象** 的平台契约，再 resume。

## 2. 静态锁定（结合包 + 本机）

```text
family app=2 → 30E15E → 30CBBC → mrc_timerCreate(305404)
            → STR timer → ERW+0x8C4
            → 顺带 30D136 → 3054A4(callback=2F5405)

family app=9 → 仅周期空转（host 以前只发这个）
family app=0xC0 → 30DC3C/30DC44 → 2FEBBC（仍不注入）

2F5390 / resume 读的 timer 槽 = ERW+0x8C4
```

## 3. modified files

| 文件 | 作用 |
|------|------|
| `bridge.c` | `jjfb_lifecycle_family_app()`；hooks `30CBBC`/`305404`；register 日志带 `ERW8C4` |
| `bridge.h` / `vmrp.c` | init 后先 `family app=2`，再 `mrc_resume(5)` |
| `RUN_V60_FAMILY_APP2_TIMER.ps1` | 跑测 |
| `scripts/v60_analyze_family_app2_timer_log.py` | 分析 |

## 4. new env vars

```text
JJFB_FAMILY_APP2_AFTER_INIT=1   # default on
JJFB_MRC_RESUME_AFTER_INIT=1    # keep
```

## 5. run command

```powershell
.\RUN_V60_FAMILY_APP2_TIMER.ps1 -Seconds 25 -SkipResourceCopy
```

## 6. key logs

```text
family app=2 → fn_30CBBC → mrc_timerCreate ×2
ERW+0x8C4 timer=0x2A8340 OK
BL_3054A4 ... r0=0x2A8340 r3=0x2F5405 CALLBACK  （app2 路径内已注册）
mrc_resume(5) ret=0  （无 timer err:1000）
entry_2F5404 #1
```

| 探针 | v59 | v60 |
|------|----:|----:|
| timer err:1000 | 1 | **0** |
| ERW+0x8C4 | 0 | **OK** |
| entry_2F5404 | 0 | **1** |
| gate / writer / C0 | 0 | 0 |

## 7. conclusion

黑屏上游再进一步：

1. 只 resume 不够，必须先有 **family app=2** 创建 timer。
2. app=2 本身就会注册 `2F5405`；resume 变为加固/二次注册。
3. `entry_2F5404` 已出现，但 25s 内只有 1 次，且尚未到 `2DADC4`/writer。

## 8. disproved assumptions

- ~~resume 单独即可完成注册~~：缺 timer → err 1000。
- ~~需要注入 C0 才能创建 timer~~：app=2 即可。

## 9. next minimal task

v61：弄清 `2F5404` 为何只进 1 次、如何持续调度到 `305EB8`/`2DADC4`/writer；仍禁止 FORCE ui_mode / 注入 C0 / host 画 UI。
