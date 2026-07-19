# 最新路线：v59 mrc_resume 打通注册

## 已突破

```text
mrc_init(0) → mrc_resume(5) → 304B5A → 2F5390 → 3054A4(r3=0x2F5405)
```

开关：`JJFB_MRC_RESUME_AFTER_INIT=1`（可用 `=0` 关掉对照）。

## 当前 blocker

```text
已注册 2F5405，但 entry_2F5404 = 0
→ 回调未被调度 → 仍无 C0 / writer
```

## v60

查 `3054A4` 登记目标与 host 调度（timer / 10140 / plat），补齐调用契约。

仍禁止：FORCE ui_mode / 注入 C0 / host 画 UI。

证据：`reports/v59_mrc_resume_lifecycle_*.md`
