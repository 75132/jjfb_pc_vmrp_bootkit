# v59 mrc_resume Lifecycle 实施报告

## 1. goal

v58 锁定：注册路径 = robotol `ext_call code=5`（mrc_resume）→ `304B5A` → `2F5390`。

v59：在 `mrc_init` 成功后按平台契约单次调用 `mrc_resume`，观察注册是否打通。

```text
不是 FORCE ui_mode
不是注入 family C0
不是 host 画 UI
是补 MRP 生命周期 resume
```

## 2. modified files

| 文件 | 作用 |
|------|------|
| `vmrp.c` | init 成功后 `bridge_dsm_ext_call(uc, 5, …)`；`JJFB_MRC_RESUME_AFTER_INIT=0` 可关 |
| `RUN_V59_MRC_RESUME_LIFECYCLE.ps1` | 跑测 |
| `scripts/v59_analyze_mrc_resume_log.py` | 分析 |

## 3. new env vars

```text
JJFB_MRC_RESUME_AFTER_INIT=1   # default on; set 0 to skip
```

## 4. run command

```powershell
.\RUN_V59_MRC_RESUME_LIFECYCLE.ps1 -Seconds 25 -SkipResourceCopy
```

## 5. key logs

```text
ext_call code=5 ...
BL_3053B8 #1
call_2F5390_prep
BL_3054A4 ... r3=0x2F5405 CALLBACK_2F5404
mrc_resume(5) after init ret=0
```

| 探针 | 次数 |
|------|-----:|
| mrc_resume | 1 |
| BL_3053B8 / 2F5390 / BL_3054A4 | 1 / 2 / 1 |
| CALLBACK register (含 2F5405) | 3 |
| entry_2F5404 | **0** |
| C0 / writer | **0** |

## 6. conclusion

**注册链已打通。** `mrc_resume` 是正确的 GWY/平台生命周期补齐。

仍未进入：`2F5404` 回调执行、`family C0`、`2DADC4`/writer。

## 7. disproved assumptions

- ~~需要注入 C0 才能注册~~：resume 即可注册。
- ~~304AEC 死了~~：一直活着，缺的是 code=5。

## 8. current blocker

`3054A4` 已登记 `0x2F5405`，但 **25s 内从未 `entry_2F5404`** → host/平台未调度该回调（或周期条件未满足）。

## 9. next minimal task

v60：弄清 `3054A4` 把 `2F5405` 登记到何处，host timer/`0x10140`/plat 谁应调用它；补齐调度契约（仍禁止 FORCE ui_mode / 注入 C0 / host 画 UI）。
