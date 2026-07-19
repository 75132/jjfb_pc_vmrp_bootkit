# v57 C0 / Callback Source Coverage 实施报告

## 1. goal

v56 证明：family 活着但全是 `app=9`；callback 注册 / `2DADC4` / writer 均为 0。

v57 只观察**上游生产者**是否执行：

```text
call sites → 2F5390 / BL 3054A4（注册 2F5405）
MOVS/CMP #0xC0（应产生 family app=0xC0）
```

禁止：FORCE ui_mode / 注入 C0 / 注入 event 5·12 / host 画 UI。

## 2. modified files

| 文件 | 作用 |
|------|------|
| `runtime/.../bridge.c` | `[JJFB_V57_SRC]` CODE hooks：`304418`/`3053BA`、`BL 3054A4` 四站点、`31309C`、`2EDA48`/`2F5D8E` |
| `scripts/v57_map_c0_callback_sources.py` | 静态图 |
| `scripts/v57_analyze_c0_callback_source_log.py` | 动态计数（已修 install banner 误计） |
| `RUN_V57_C0_CALLBACK_SOURCE.ps1` | 自然跑 25s |

## 3. new env vars

无新开关。沿用：

```text
JJFB_GWY_LAUNCHER_MODE=1
JJFB_FORCE_UI_MODE=0
JJFB_DISABLE_MRC_EVENT0_INJECT=1
```

## 4. run command

```powershell
.\RUN_V57_C0_CALLBACK_SOURCE.ps1 -Seconds 25 -SkipResourceCopy
```

## 5. key logs

```text
[JJFB_V57_SRC] coverage installed ... NO_INJECT NO_FORCE
[JJFB_V56_FAMILY] dispatch #N app=0x9 ... lr=0x80000   (N≥400)
# 无 call_2F5390_prep / BL_3054A4 / cmp_r0_C0 / movs_imm_C0
```

| 探针 | 次数 |
|------|-----:|
| call 2F5390 prep | 0 |
| BL 3054A4 | 0 |
| MOVS/CMP #0xC0 | 0 |
| family (max #) | ≥400，全 app=9 |
| family app=0xC0 | 0 |
| callback / writer | 0 |

## 6. conclusion

注册链与 C0 生产者**整段未进入**。不是“已注册未调度”。

`lr=0x80000` = host `jjfb_flush_1e200` 调 guest family；当前只 flush `app=9`。

## 7. disproved assumptions

- ~~BL 3054A4 曾命中~~：分析器误把 install 横幅里的 `BL_3054A4` 算作命中；修正后为 0。
- ~~“只要 family 活着就会走到 C0”~~：400+ 次仍全是 app=9。

## 8. current blocker

谁应进入：

```text
0x304AEC → 304B5A → 3053B8 → 2F5390 → 3054A4
0x2ED8E4 / 0x2E8C00 / 0x2F5CA4（含 movs #0xC0）
```

这些函数无普通字面量入口；部分靠跳表/间接调用。

## 9. next minimal task

v58：对上述 **fn entry** 加观察 hook（仍禁止注入），定位卡在哪一层；对照 GWY `startGame` 契约是否缺某次 `0x1E209(app≠9)` 或其它 plat 回调。
