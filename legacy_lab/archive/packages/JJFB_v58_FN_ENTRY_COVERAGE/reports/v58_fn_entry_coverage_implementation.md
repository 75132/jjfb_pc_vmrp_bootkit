# v58 Fn-Entry Coverage 实施报告

## 1. goal

v57：注册/C0 生产者 PC 全 0。v58 观察上一层 fn entry 是否进入，并静态解释分支。

禁止 FORCE / 注入 C0·5·12 / host 画 UI。

## 2. modified files

| 文件 | 作用 |
|------|------|
| `bridge.c` | `[JJFB_V58_FN]` hooks：`304AEC`/`3053B8`/`2ED8E4`/`2E8C00`/`2F5CA4`/`2DB044`/`2EB770` + BL sites |
| `RUN_V58_FN_ENTRY_COVERAGE.ps1` | 自然跑 |
| `scripts/v58_analyze_fn_entry_log.py` | 分析 |

## 3. new env vars

无。

## 4. run command

```powershell
.\RUN_V58_FN_ENTRY_COVERAGE.ps1 -Seconds 25 -SkipResourceCopy
```

## 5. key logs / counts

| 探针 | 结果 |
|------|------|
| `fn_304AEC_reg` | **457**（lr=0x80000，几乎全是 `r1=code=2` timer） |
| 首次 | `code=0` init |
| `BL_3053B8` / `2F5390` / C0 | **0** |
| 其它 C0 fn | **0** |

## 6. conclusion（锁定）

`0x304AEC` = robotol helper 入口（`0x304AED`）。跳表：

| ext code | 含义（MRP ABI） | 落点 | 本机是否调用 |
|---:|---|---|---|
| 0 | mrc_init | `304B1C` | 是 |
| 1 | mrc_event | `304B30` | 否（`DISABLE_MRC_EVENT0_INJECT=1`） |
| 2 | timer | `304B4A` → `30560C` | 是（大量） |
| **5** | **mrc_resume** | **`304B5A` → `3053B8` → `2F5390` → 注册** | **否** |
| 6/8 | 801 契约 | 其它 | 是 |

**注册链未启动的直接原因：host 从未 `bridge_dsm_ext_call(..., code=5)`（mrc_resume）。**

## 7. disproved assumptions

- ~~304AEC 从未执行~~：一直在跑，只是停在 timer 分支。
- ~~需要注入 family app=0xC0 才能注册~~：注册入口是 resume 生命周期，不是 C0。

## 8. current blocker

缺平台生命周期 **`mrc_resume`（ext code=5）**；C0 生产者链仍未进入（可能 resume 之后才走）。

## 9. next minimal task

v59：在 `mrc_init(code=0)` 成功后，按平台契约 **单次** `ext_call code=5`（mrc_resume），观察是否命中 `304B5A`/`2F5390`/`3054A4`。仍禁止 FORCE ui_mode / 注入 C0 / host 画 UI。
