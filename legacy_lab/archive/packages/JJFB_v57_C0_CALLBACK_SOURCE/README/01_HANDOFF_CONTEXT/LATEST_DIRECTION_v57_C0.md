# 最新路线：v57 C0 / Callback Source（本机已跑）

> 与 `LATEST_DIRECTION_v57_CORRECTED.md`（禁止 host 自绘）并存：那是 UI 禁令；本文件是上游触发主线。

## 本机动态结论（25s，无 FORCE / 无注入）

| 探针 | 次数 |
|------|------|
| call 2F5390 prep / BL 3054A4 | **0** |
| MOVS/CMP #0xC0 | **0** |
| family dispatch | ≥400（全 **app=9**，lr=0x80000 host flush） |
| family app=0xC0 | **0** |
| callback / 2DADC4 / writer | **0** |

判定：注册链与 C0 生产者从未启动。

## 静态下一层（已标）

```text
注册: 304AEC → 304B5A → 3053B8 → 2F5390 → 3054A4
C0:   2ED8E4 ← 2E8C9A/2E8CD8/30DAE8；2E8C00 ← 2EB8AA/30DEE6
      2F5CA4 ← 2DB084
```

## v58 唯一任务

观察上述 **fn entry** 谁从未进入；仍禁止 FORCE / 注入 C0·5·12 / host 画 UI。

证据：`reports/v57_c0_callback_source_*.md`、`logs/v57_c0_callback_source_stdout.txt`
