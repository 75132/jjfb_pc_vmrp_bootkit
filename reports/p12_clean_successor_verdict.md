# P12 Clean Golden Run + Case-9 Successor Verdict

## Bottom line

Case-9 remains closed under product ABI locks. Clean rebuild on `5805498` with frozen binaries ran three product goldens; all three naturally returned Case-9 (`DELIVER_DONE ok=1` / `handler=0x30D311`). The first real post-`CASE9_LEAVE` activity is `_mr_c_function_new` → load `mmochat.ext` (`gwy/wxjwq.mrp`), then `mr_drawBitmap(bmp=0)`, then hard stop on unimplemented **`mr_getCharBitmap` @ slot `0x28007C`** (bridge `exit(1)` → DSM reinit). Stagnation type **A**.

## G0 identity

| Field | Value |
|------|--------|
| source_commit | `58054980030fdddf082cb902b8fb17e9168bca19` |
| product src clean at rebuild | yes |
| main.exe SHA256 | `6870a17c0b75915b36798fd860f00b5c59b48f17b0564d92206b7907213836f1` |
| JJFB_Launcher.exe SHA256 | `74a8ec83819c1197d09f995f214414c8987a2755469934d7297bdcf2898dabbe` |
| gwy_launcher.exe SHA256 | `d50f89fbf65090ce8563af67f74c1fda71634b3a344405d12d28ad008dd1d229` |
| compiler | gcc-16.1.0 |
| build | `RUN_BUILD.ps1` + `RUN_BUILD_VMRP.ps1 -Mode Gwy` |
| tests | product unit suite OK (audit_launcher_core still reports pre-existing absolute-code findings) |
| ABI locks | `direct_lr`, `apply_abi=0`, `EVENT_CONTRACT=0`, `FAMILY_4F_FOR_E6C=0` |
| identity file | `out/p12/p12_clean_build_identity.txt` (not used to dirty tracked reports) |

Triple runs used the same frozen SHA (no mid-run rebuild).

## Triple golden matrix (summary)

| run_id | strong | Case-9 leave | P3_FAULT | post_callback | BMP count | first_frame_sha |
|--------|--------|--------------|----------|---------------|-----------|-----------------|
| p2_20260801_045052_71623 | yes | yes | no | FIRST_NATURAL_DRAW | 0 | NOT_SEEN |
| p2_20260801_045130_67067 | yes | yes | no | FIRST_NATURAL_DRAW | 0 | NOT_SEEN |
| p2_20260801_045208_75950 | yes | yes | no | NATURAL_CALLBACK_IDLE_WAIT | 0 | NOT_SEEN |

Notes:

- `FIRST_NATURAL_DRAW` here is `mr_drawBitmap` with **NULL bmp** (`DRAW_API_WITH_NULL_BMP`), not a Layer-1 / five-BMP splash frame.
- Run 3 hit the product script’s early stop (sched+5s) before the draw marker landed in the gate window; Case-9 leave still present. Strong gates all yes ×3.
- Five natural BMPs: **not observed** in this product window (same as P11 `NOT_SEEN`).

Full CSV: `reports/p12_clean_run_matrix.csv`.

## Post-Case-9 timeline (T0 = first `CASE9_LEAVE ok=1`)

| seq | offset | event |
|-----|--------|-------|
| 1 | 0 | `CASE9_LEAVE ok=1` `pc_after=0x80000` `ret=0` |
| 2 | +4 | `DELIVER_DONE` handler=`0x30D311` lr=`0x304599` |
| 3 | +5 | `SCHEDULER_NATURAL_CALLBACK` forced=no |
| 4 | +6 | extchunk/sendAppEvent echo `r0=0x1E209 r1=0x9` |
| 5 | +10 | `_mr_c_function_new` (robotol continuation `0x304599`) |
| 6 | +22 | module switch: `gwy/wxjwq.mrp` / `mmochat.ext` MAPPED→REGISTERED |
| 7 | +43 | `mr_drawBitmap bmp=0x0` → `DRAW_API_WITH_NULL_BMP` |
| 8 | +47 | **`mr_getCharBitmap` UNIMPLEMENTED** slot=`0x28007C` |
| 9 | +57 | `initMemoryManager` / DSM restart (bridge `exit(1)` path) |

Artifacts: `reports/p12_post_case9_timeline.csv`, `reports/p12_post_case9_hot_blocks.csv`, `logs/p12_successor_stdout.txt`.

### Firsts after Case-9 leave

| probe | result |
|-------|--------|
| first `0x101xx` platform call | none in immediate window (next hard stop is MAP_FUNC, not 0x101xx) |
| first sendAppEvent / extchunk | echo of family event (`0x1E209`/`9`) then `_mr_c_function_new` |
| first resource request | none (no new BMP req); module extract of `mmochat.ext` |
| first timer callback | none |
| first scheduler callback | immediate `SCHEDULER_NATURAL_CALLBACK` (deliver ack) |
| first network | none |
| first module switch | `mmochat.ext` from `gwy/wxjwq.mrp` |
| first exception/exit/park | `mr_getCharBitmap` → process exit → reinit |

## Type A detail — missing platform service

```text
第一个缺失平台服务: mr_getCharBitmap
调用地址 / slot:     0x28007C  (bridge MAP_FUNC offset 0x78)
参数:                (observed via unimplemented trap; char/font/width/height per mrporting)
调用来源:            post-Case-9 continuation after mmochat.ext c_function_new;
                     draw path at pc≈0x280078 / lr=0x304599 (robotol continuation)
预期返回语义:        const char* glyph bitmap + width/height outs (mythroad text path)
当前宿主行为:        gwy_ext_obs_unimplemented_api → printf → exit(1)
                     (unless E10A shell_continued soft-return; product path uses exit)
```

## Required answers

```text
能否出现真实游戏画面：否（仅有 NULL-bmp draw 候选；无 Layer-1 / DispUp 实帧）
第一帧是否三次稳定：否（无 first_frame_sha；post_callback 2×DRAW / 1×IDLE_WAIT）
自然资源数量：0（本窗口未见到 JJFB_BMP_REQ；五 BMP 未保持/未出现）
Case-9 是否自然返回：是（三次 DELIVER_DONE/CASE9_LEAVE ok=1）
Case-9 后第一个平台行为：_mr_c_function_new → 映射 mmochat.ext，随后 mr_drawBitmap(NULL) → mr_getCharBitmap
当前停滞类型：A
当前唯一阻塞点：mr_getCharBitmap @ 0x28007C（未实现 MAP_FUNC，触发 exit/reinit）
下一处最小通用平台缺口：实现通用 mr_getCharBitmap（字体位图服务），不要回到 Case-9/资源解析器
是否比 P11 更接近登录界面：是（门锁已从 Case-9 ABI 前移到文本/字形平台 API）
```

## P12 gate checklist

1. clean source commit — yes (`5805498`; P12 runners/reports committed with this close)
2. EXE ↔ commit identity — yes (`out/p12/p12_clean_build_identity.txt` + freeze)
3. three runs same binary — yes (SHA freeze enforced)
4. three runs comparable — yes (matrix)
5. real first frame? — no / NOT_SEEN
6. five natural BMPs? — no / 0
7. Case-9 natural return — yes ×3
8. no host 5th/6th params — held
9. first natural post-Case-9 activity — found (`mmochat` load + `mr_getCharBitmap`)
10. unique blocker — `mr_getCharBitmap`
11. next fix — implement that platform MAP_FUNC

## Freeze / next

- Do not reopen `0x1E205` / `0x2D960E` / Case-9 stack ABI.
- Do not inject sixth resource / Event15 / Family 4F.
- Next minimal work: generic `mr_getCharBitmap` implementation (or documented soft-fail contract that does not `exit(1)` on product path), then re-run clean golden to see the next natural request.
