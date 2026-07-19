# v56 GWY Bring-up 运行结果（白屏）

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v56_gwy_bringup_stdout.txt`
- 行数：1805

## 1. 目标

- 解决白屏：平台 dims/C44 + guest `0x2FC03C`→必要时 `0x2FC418` + DispUpEx。
- 不做 host `uc_mem_write` FORCE ui_mode。

## 2. 检查项

- handoff+timer：是
- natural_mode（无 FORCE 写）：是
- FORCE mem-write 出现：否
- GWY_BRINGUP：是
- presented dark baseline：是
- uimode_writer ENTER：是
- call 0x2FC418：是
- ui_mode after 2FC03C：`0x3`
- ui_mode after 2FC418：`0x45`
- ui_mode final：`0x45`
- C44 final：`1`
- SPLASH_ENTER：否
- DispUpEx 次数：0
- JJFB_DRAW 次数：14
- host blit/present：是

## 3. 关键日志

```text
[JJFB_GWY_BRINGUP] contract=dims+C44+guest_2FC03C_writer+DispUpEx no_FORCE_memwrite
[JJFB_GAME_SELF] uimode_writer coverage installed (2FC418/2FC03C/2DADC4/B70/callers)
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer period=50 RUNNING=1
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_GWY_BRINGUP] contract=dims+C44+guest_2FC03C_then_2FC418+DispUpEx no_FORCE_memwrite
[JJFB_GWY_BRINGUP] start ui_mode=0x0 (call guest writer, not FORCE write)
[JJFB_GWY_BRINGUP] seed screen 240x320 + hwD14=1
[JJFB_GWY_BRINGUP] call 0x2FC8B8 enable C44
[JJFB_GWY_BRINGUP] C44=1 after 2FC8B8
[JJFB_GWY_BRINGUP] call 0x2FC03C (may not BL writer; expect probe)
[JJFB_DEBUG_PRESENT] from DrawRect dirty 0,0 240x320 (host debug; DispUpEx/mrc_refreshScreen untouched)
[JJFB_GWY_BRINGUP] ui_mode 0x0 -> 0x3 after 2FC03C
[JJFB_GWY_BRINGUP] call 0x2FC418 natural writer (ui_mode!=0x45)
[JJFB_GAME_SELF] uimode_writer ENTER #1 pc=0x2FC418 lr=0x80000 ui_mode=0x3 (will store 0x45)
[JJFB_GAME_SELF] uimode_writer STORE #1 pc=0x2FC448 lr=0x2D9AD3 r0=0x45 ui_mode_before=0x3
[JJFB_GWY_BRINGUP] ui_mode 0x3 -> 0x45 after 2FC418
[JJFB_GWY_BRINGUP] C44 cleared; re-call 0x2FC8B8
[JJFB_GWY_BRINGUP] final ui_mode=0x45 C44=1
[JJFB_GWY_BRINGUP] presented dark baseline (DispUpEx also live)
[JJFB_DEBUG_PRESENT] from DrawRect dirty 0,0 240x320 (host debug; DispUpEx/mrc_refreshScreen untouched)
```

## 4. 结论

- bring-up 成功：guest writer 写到 0x45，C44 打开，已 present 非白底。

## 5. blocker / 下一步

- blocker: splash content may still be sparse (progress/AC8)
- next: 若仍接近空白，追 splash host-blit / 自然 progress；并行追 2DADC4 自然 caller。
