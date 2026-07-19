# 最新路线：v62（对照 v61）

## 已落地

```text
v61: nest-fix → 持续 2F5404 → 305EB8（旧日志误报 B70=0）
v62: 门控实为 15D==1 / B71!=0 / 134D==0
     app=2 写 15D=1、清 B71；305EB8 全部 fail_B71_eq_0
     B71=1 写者（30ED2C / 2DC4D8）运行期 0 次
```

## 当前 blocker

```text
B71 恒 0 → 进不了 305EF4 / 2DADC4 / writer
候选：MR_MOUSE_UP(3) → 2E2520 → 2DC4D8
```

## v63

审计 `MR_MOUSE_UP` / 其它自然路径是否应置 `B71`；补齐平台事件契约。  
禁止 FORCE / C0 inject / host UI；勿无事件码盲扫当正式方案。
