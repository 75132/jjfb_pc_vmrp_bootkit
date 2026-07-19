# v54 Natural Game-Self 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v54_natural_game_self_stdout.txt`
- 总行数：1211
- FILEOPEN_MISS：0

## 1. 目标

- 保留 v53：别名 hook + MR_IGNORE 后置 host 801。
- 关闭默认 FORCE ui_mode=0x45，观察原始游戏自然推进。

## 2. Handoff / Natural 门

- alias+robotol：是
- handoff 801+timer：是
- NO FORCE / natural_mode：是
- FORCE 0x45 出现：否
- `mr_get_method`：`[1514, 219, 161178]`
- ui_mode 分布：`{'0x0': 204}`
- state 分布：`{'0': 46}`

## 3. GAME_SELF 日志

```text
[JJFB_GAME_SELF] contract=GWY_no_FORCE_ui_mode observe_natural_events_network
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
```

## 4. FILEOPEN guests (top 20)

- 3 `mythroad/gwy/jjfb.mrp`
- 1 `mythroad/system/gb16.uc2`
- 1 `mythroad/sdk_key.dat`

## 5. 网络 / _strCom

```text
[JJFB_STARTUP_STR] watching connecting @0x313C5C
[JJFB_XREF_LIT] name=str_connecting total_hits=0
```
- _strCom：无

## 6. Tag counts (top 20)

- 423 `[JJFB_SEND]`
- 97 `[JJFB_ERW_WRITE]`
- 60 `[JJFB_TIMER_DISPATCH]`
- 51 `[JJFB_2EFC_DISASM]`
- 49 `[JJFB_FIRST_SCREEN]`
- 39 `[JJFB_801]`
- 38 `[JJFB_AC8TAIL_DISASM]`
- 34 `[JJFB_UI_DISPATCH]`
- 34 `[JJFB_HANDLER_306344]`
- 33 `[JJFB_XREF_LIT]`
- 31 `[JJFB_GATE_DISASM]`
- 30 `[JJFB_STARTUP_PHASE]`
- 30 `[JJFB_DEEP_DISASM]`
- 26 `[JJFB_DIM]`
- 15 `[JJFB_2EF86C_COV]`
- 14 `[JJFB_2EFC_GATE]`
- 9 `[JJFB_CTX]`
- 7 `[JJFB_XREF_DUMP]`
- 6 `[JJFB_STARTUP_STR]`
- 5 `[JJFB_LOADER]`

## 7. 关键日志

```text
mr_get_method(1514)
mr_get_method(219)
mr_get_method(161178)
[JJFB_MRP_ALIAS] contract=cfunction.ext->robotol.ext method=br_log_ext_base_0xD4 scope=guest_memory_only
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
[JJFB_MRP_ALIAS] patched ext_base=0x2AEB04 literal=0x2AEBD8 request=cfunction.ext target=robotol.ext method=ext_base_0xD4
[JJFB_ROBOTOL_LOAD] ordinal=3 guest_ordinal=2 helper=0x304AED P=0x2AC8DC ext_base=0x2D8DF4 loader_helper=0x2AEB48 after_alias=1 source=br_log
bridge_dsm_mr_start_dsm
bridge_dsm_mr_start_dsm filename=gwy/jjfb.mrp extName=start.mr entry=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
bridge_dsm_mr_start_dsm ret=0x1
bridge_dsm_mr_start_dsm('gwy/jjfb.mrp','start.mr',SRCV): 0x1
[JJFB_START_HANDOFF] contract=MR_IGNORE_only_after_alias_and_robotol then host_6_8_0
[JJFB_START_HANDOFF] raw_ret=0x1 semantic=MR_IGNORE alias_applied=1 robotol_loaded=1 helper=0x304AED action=run_host_801_recovery
[JJFB_801_GUARD] robotol_loaded=1 alias_applied=1 robotol_helper=0x304AED action=run_host_801
[JJFB_801] host version(6) ret=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer period=50 RUNNING=1
[JJFB_GAME_SELF] contract=GWY_no_FORCE_ui_mode observe_natural_events_network
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_FIRST_SCREEN] state_change 0 -> 0 (tick=1 why=pre-handler)
[JJFB_FIRST_SCREEN] ui_mode_change 0x0 -> 0x0 (tick=1) (0x45=splash/slogo)
[JJFB_FIRST_SCREEN] #1 state pre-handler
[JJFB_FIRST_SCREEN] #2 state post-handler
[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural) state=0x0 tick=10
[JJFB_FIRST_SCREEN] #3 state pre-handler
[JJFB_FIRST_SCREEN] #4 state post-handler
[JJFB_FIRST_SCREEN] #5 state pre-handler
[JJFB_FIRST_SCREEN] #6 state post-handler
[JJFB_FIRST_SCREEN] #7 state pre-handler
[JJFB_FIRST_SCREEN] #8 state post-handler
[JJFB_FIRST_SCREEN] #9 state pre-handler
[JJFB_FIRST_SCREEN] #10 state post-handler
[JJFB_FIRST_SCREEN] #11 state pre-handler
[JJFB_FIRST_SCREEN] #12 state post-handler
[JJFB_FIRST_SCREEN] #13 state pre-handler
[JJFB_FIRST_SCREEN] #14 state post-handler
[JJFB_FIRST_SCREEN] #15 state pre-handler
[JJFB_FIRST_SCREEN] #16 state post-handler
[JJFB_FIRST_SCREEN] #17 state pre-handler
```

## 8. 结论

- 成功关闭 FORCE；ui_mode 仍停在 0；下一 blocker 是自然推进事件/平台回调。

## 9. 当前 blocker

- ui_mode stuck at 0 without FORCE

## 10. 下一步最小任务

- 定位谁本应写 ERW state/ui_mode（事件、_strCom、网络），禁止 FORCE。
