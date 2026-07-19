# 最新路线：v64（对照 v63）

## 已落地

```text
v63: helper mrc_event(5) 是 no-op，不进 2E2520
v64: 找到真实入队链 0x10165 → 30D2F9 → 30D24C → 2E4D6C → B54
     host 原先把 10165 当纯 alloc，丢掉 handler（契约缺口）
     已保存 handler；PROBE 跑通 30D2F8→30D24C→2E4D6C
```

## 当前 blocker

```text
2E4D6C 入场时 r1=0 → 立即退出，B54 仍空，2E2520=0
（30D24C 未产出非空事件指针再调 2E4D6C）
```

## v65

查 30D24C 为何 `[sp,#0xc]=0`；补齐 10165/调用 ABI（buf 内容或其它前置），
让 2E4D6C 以非空 r0/r1 真正 push，再被 2DC80C drain。
禁止 FORCE / C0 inject / host UI / mrc_event 盲扫。
