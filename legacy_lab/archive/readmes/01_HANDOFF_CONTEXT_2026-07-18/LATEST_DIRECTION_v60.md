# 最新路线：v60（对照 LIFECYCLE 包）

## 包结论已落地

`JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE`：

- 注册：method1+10002 **或** method5。
- 本机补丁：method5 前必须有 timer。

## 本机突破

```text
init → family app=2 → timerCreate → ERW+0x8C4
     →（app2 内）3054A4(2F5405)
     → mrc_resume(5) 无 timer err:1000
     → entry_2F5404 ×1
```

仍无：`2DADC4` / writer / family C0 → 故仍可能黑屏。

## v61

持续调度 `2F5404` → `305EB8` → `2DADC4` → `2FC418`。

禁止：FORCE ui_mode / 注入 C0 / host 画 UI。
