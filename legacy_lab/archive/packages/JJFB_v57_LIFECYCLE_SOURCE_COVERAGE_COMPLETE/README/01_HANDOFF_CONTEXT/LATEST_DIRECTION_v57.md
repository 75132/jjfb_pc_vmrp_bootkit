# LATEST DIRECTION — v57

## 已锁定

- v52 alias 与 v53 handoff 已完成。
- v55 锁定自然 writer `0x2FC418`，但 25 秒内没有上游调用。
- v56 本机实测：family 只有 `app=9`；事件链、callback 注册链、`2DADC4` 均未启动。

## v57 唯一任务

1. 证明 `family app=0xC0` 的真实输入边界；区分 guest `0x1E209` 发送与 platform-originated family callback。
2. 证明 `0x3054A4` 注册 `0x2F5405` 的两个自然生产者：
   - robotol EXT method 1 + lifecycle command `10002`；
   - robotol EXT method 5。
3. 动态确认当前运行中哪一条最先缺失。

## 严格禁止

- 不注入 `app=0xC0`。
- 不注入 EXT method 1/5 或 command 10002。
- 不 FORCE `ui_mode=0x45`。
- 不启用 AC8/progress driver。
- 不由 host 绘制游戏 UI。
