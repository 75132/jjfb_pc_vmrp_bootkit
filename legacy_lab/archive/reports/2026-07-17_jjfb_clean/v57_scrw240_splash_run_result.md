# v57 SCRW240 / 10134 / splash 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v57_scrw240_splash_stdout.txt`
- 行数：1881

## 1. 目标

- 结合 `docx`：SCRW=240、绘图后 refresh。
- 修复 10134 对 splash 尺寸 no-match；DrawRect 320 布局 remap；present 原版资源。
- 不做 host FORCE ui_mode。

## 2. 检查项

- handoff+timer：是
- FORCE mem-write：否
- size-map 次数：3
- 10134_BMP 次数：3
- BMP_LOAD 次数：3
- axis_remap 次数：15
- splash present：是
- ui_mode 0x45：是

## 3. 关键日志

```text
[JJFB_GWY_SPLASH] contract=docx_SCRW240+10134_size_map+axis_remap+original_blit no_FORCE
[JJFB_801] host mrc_init(0) ret=0
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_10134] size-map bytes=0x2D8A -> loadingbar!201!29.bmp
[JJFB_BMP_LOAD] cache jjfb_bmp_cache/loadingbar!201!29.bmp.rgb565 -> 11658 bytes
[JJFB_10134_BMP] #1 name=loadingbar!201!29.bmp w=201 h=29 bytes=0x2D8A guest_pixels=0x6D95CC loaded=1
[JJFB_10134] size-map bytes=0x240 -> bar!16!18.bmp
[JJFB_BMP_LOAD] cache jjfb_bmp_cache/bar!16!18.bmp.rgb565 -> 576 bytes
[JJFB_10134_BMP] #2 name=bar!16!18.bmp w=16 h=18 bytes=0x240 guest_pixels=0x6DC35C loaded=1
[JJFB_10134] size-map bytes=0x1C20 -> textbar!120!30.bmp
[JJFB_BMP_LOAD] cache jjfb_bmp_cache/textbar!120!30.bmp.rgb565 -> 7200 bytes
[JJFB_10134_BMP] #3 name=textbar!120!30.bmp w=120 h=30 bytes=0x1C20 guest_pixels=0x6DC5E4 loaded=1
[JJFB_GWY_BRINGUP] final ui_mode=0x45 C44=1
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_BLIT_KEYED] name=loadingbar!201!29.bmp x=19 y=220 w=201 h=29 drawn=5627 skipped=202 tag=GWY_SPLASH
[JJFB_DIRTY_RECT] tag=GWY_SPLASH name=loadingbar!201!29.bmp x=19 y=220 w=201 h=29
[JJFB_BLIT_KEYED] name=bar!16!18.bmp x=112 y=210 w=16 h=18 drawn=152 skipped=136 tag=GWY_SPLASH
[JJFB_DIRTY_RECT] tag=GWY_SPLASH name=bar!16!18.bmp x=112 y=210 w=16 h=18
[JJFB_BLIT_KEYED] name=textbar!120!30.bmp x=60 y=240 w=120 h=30 drawn=3600 skipped=0 tag=GWY_SPLASH
[JJFB_DIRTY_RECT] tag=GWY_SPLASH name=textbar!120!30.bmp x=60 y=240 w=120 h=30
[JJFB_GWY_SPLASH] presented 3 original assets (refresh after draw)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
[JJFB_DRAW] axis_remap x 270 -> 190 (320-layout -> SCRW=240)
```

## 4. 结论

- docx 路径成立：SCRW240 + 原版 splash 资源已 load/present；无 FORCE。

## 5. blocker / 下一步

- blocker: observe natural 2EC6B0 / 2DADC4 callers next
- next: 确认画面可见；并行追 2DADC4 自然 caller。
