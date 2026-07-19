# v62 B70/B71/15D Gate Coverage — Implementation

## 1. goal

v61 已持续进入 `305EB8`，但旧探针只看 `B70=0`。  
本轮弄清真实门控与写者（观察 only）：

```text
305EB8 → 2DADC4 需要:
  ERW+0x15D == 1
  ERW+0xB71 != 0
  ERW+0x134D == 0
```

禁止 FORCE ui_mode / 注入 C0 / host 画 UI。

## 2. modified files

| 文件 | 作用 |
|------|------|
| `bridge.c` | 丰富 `entry_305EB8`（15D/B71/fail tag）；写者 code hook；mem watch |
| `scripts/v62_static_b70_305eb8.py` | 静态图 |
| `scripts/v62_analyze_b70_gate_log.py` | 日志分析 |
| `RUN_V62_B70_GATE_COVERAGE.ps1` | 跑测 |
| `reports/v62_b70_305eb8_static_map.md` | 静态结论 |
| `README/01_HANDOFF_CONTEXT/LATEST_DIRECTION_v62.md` | 路线 |

## 3. new env vars

无（沿用 v60/v61）。

## 4. run command

```powershell
.\RUN_V62_B70_GATE_COVERAGE.ps1 -Seconds 25 -SkipResourceCopy
```

## 5. key logs

```text
[JJFB_V62_FLAG] writer #1 ... strb_15D=1_in_30CBBC
[JJFB_V62_FLAG] mem_write ERW+0x15D new=0x1
[JJFB_V62_FLAG] writer #2 ... strb_B71=0_in_2FE82C
[JJFB_V62_FLAG] after family app=2 15D=1 B71=0 B70=0 134D=0
entry_305EB8 #N ... 15D=1 B71=0 ... fail_B71_eq_0
```

| 探针 | 次数 |
|------|-----:|
| 30CCF4 15D=1 | 1 |
| 2FE854 B71=0 | 1 |
| 30ED7A / 2DC572 B71=1 | **0 / 0** |
| fail_B71 | 全部 entry |
| ok_to_2DADC4 / 305EF4 | **0** |

## 6. conclusion

1. **旧假设证伪**：`305EB8` 不看 `B70`；看 `15D` / `B71` / `134D`。
2. **15D 已满足**：`family app=2` → `30CBBC` → `30CCF4` 写 `15D=1`；`134D=0`。
3. **blocker = B71**：同路径 `2FE82C` 把 `B71` 清 0 后，25s 内 **没有任何写者再置 1**。

静态补充：`2E2520` 跳表 index0（`event_code-3==0` → **`MR_MOUSE_UP=3`**）→ `BL 0x2DC4D8`（可写 B71）。  
另一写者 `30ED2C` 主要从已进入的 `2DADC4` 尾部调用（鸡生蛋）。

## 7. disproved assumptions

- ~~305EB8 卡在 B70~~：卡在 **B71==0**；B70 是更后面的 `2DADC4` 内部门。
- ~~缺 C0 / 缺 resume 才进不了 2DADC4~~：Path C 已到 305EB8，缺的是 B71 置位契约。

## 8. current blocker

`ERW+0xB71` 在 app=2 初始化后恒 0；`30ED2C` / `2DC4D8` 均未命中。

## 9. next minimal task

v63：弄清谁应把 `B71=1`——优先审计 `MR_MOUSE_UP`→`2E2520`→`2DC4D8` 是否为平台应投递事件，或是否有其它自然路径；仍禁止 FORCE ui_mode / 注入 C0 / host 画 UI（事件投递需有契约证据，勿盲扫）。
