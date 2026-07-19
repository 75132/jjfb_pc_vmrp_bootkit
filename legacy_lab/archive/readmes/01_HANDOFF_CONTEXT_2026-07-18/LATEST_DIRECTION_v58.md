# 最新路线：v58 Fn-Entry（已锁定 resume 缺口）

## 结论

robotol helper `0x304AED` 在跑，但 host 只发 `code=0/2/6/8`。

**`code=5`（mrc_resume）→ `304B5A` → `3053B8` → `2F5390` → 注册 `2F5405`。**  
本机从未发 code=5，故注册链为 0。

## 禁止

```text
FORCE ui_mode / 注入 C0 / 注入 event 5·12 / host 画 UI
```

## v59

```text
mrc_init 成功后，平台契约单次 mrc_resume (ext_call code=5)
→ 观察 2F5390 / 3054A4 / family C0 / writer
```

证据：`reports/v58_fn_entry_coverage_*.md`
