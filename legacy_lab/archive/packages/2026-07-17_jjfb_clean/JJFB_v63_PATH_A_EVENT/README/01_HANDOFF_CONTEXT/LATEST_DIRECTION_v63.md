# 最新路线：v63（对照 v62）

## 已落地

```text
v62: 305EB8 卡 B71；15D=1 已满足
v63: MOUSE_UP 证伪（134D=2）
     Path A 静态成立：event 5/12 → 2E4040 → 2DADC4
     PROBE helper code=1 mrc_event(5) ret=0 但不进 2E2520
```

## 当前 blocker

```text
host→robotol 事件入队契约未知（code=1 ≠ 2E2520 路径）
```

## v64

谁调用 `2DC8D4`/`2E7B9E`→`2E2520`；补齐正确 Path A 投递。  
禁止 FORCE / C0 inject / host UI / 事件盲扫。
