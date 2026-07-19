# v55 ui_mode Writer Coverage 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v55_uimode_writer_stdout.txt`
- 行数：1189

## 1. 目标

- 定位自然写 `ERW+0x8D0=0x45` 的 guest 函数，不用 FORCE。
- 覆盖：`0x2FC418` writer ← `0x2FC03C` ← `0x2DADC4`(B70) ← callers。

## 2. 门禁

- handoff 801+timer：是
- NO FORCE / natural_mode：是
- writer coverage installed：是
- writer ENTER：0
- writer STORE：0
- 2DADC4：0
- B70 check：0
- BL 2FC03C：0
- init 2FC03C：0
- callers：0
- alt 30EE50：0

## 3. GAME_SELF 样本

```text
[JJFB_GAME_SELF] contract=trace_natural_uimode_writer_2FC418 no_FORCE
[JJFB_GAME_SELF] uimode_writer coverage installed (2FC418/2FC03C/2DADC4/B70/callers)
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
```

## 4. 结论

- handoff+natural+coverage OK，但 writer 链全程未执行。

## 5. 当前 blocker

- no caller of 2DADC4/2FC418 in 25s natural run

## 6. 下一步

- 追谁应调用 2DADC4（2FECA2/2E4066/305EF4），及其前置平台事件。
