# 最新路线：v61（对照 LIFECYCLE 包）

## 已落地

```text
LIFECYCLE: method5 需 timer
v60: app=2 → timerCreate → resume → entry_2F5404×1（随后嵌套挂死）
v61: ext_call 内 defer 1E209 → 持续 2F5404 → 2F5734 → 305EB8
```

## 当前 blocker

```text
305EB8 每次 B70=0 → 未到 305EF4 / 2DADC4 / writer
```

## v62

谁写 B70；`305EB8` 分支条件。禁止 FORCE / C0 inject / host UI。
