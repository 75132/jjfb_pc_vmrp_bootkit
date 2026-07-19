# v53 Start Handoff Recovery 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v53_start_handoff_recovery_stdout.txt`
- 总行数：1617
- FILEOPEN_MISS：0
- 原始 MRP SHA256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- SDK key SHA256：`5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436`

## 1. Loader 与别名

- SDK 报错：否
- `start.mr` 1514：是
- `mrc_loader.ext` 219：是
- alias `ext_base+0xD4`：是
- alias patched：是
- `err code=3006`：否
- `robotol.ext` 161178：是
- robotol helper：是

## 2. Start handoff / 801

- `start_dsm` 原始返回：0x1
- MR_IGNORE 恢复门：是
- stop_before_host_801：否
- 801 guard 放行：是
- 801 guard 阻止误判：否
- host version(6)：0
- host appInfo(8)：0
- host mrc_init(0)：0
- robotol timer RUNNING：是
- timer code=2：是

## 3. 顺序

- `mr_get_method`：`[1514, 219, 161178]`

## 4. 自动结论

- v53 目标完成：原始 start 返回 MR_IGNORE 后，host 6→8→0 恢复成功并启动 robotol timer。

## 5. 关键日志

```text
[JJFB_MRP_ALIAS] contract=cfunction.ext->robotol.ext method=br_log_ext_base_0xD4 scope=guest_memory_only
[JJFB_START_HANDOFF] contract=MR_IGNORE_only_after_alias_and_robotol then host_6_8_0
ext call _mr_c_function_new(0xA4088[671880], 0x14[20])
[JJFB_STARTGAME] startGame/runapp equivalent=bridge_dsm_mr_start_dsm
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
[JJFB_LOADER] bridge_dsm_mr_start_dsm filename=gwy/jjfb.mrp extName=start.mr entry=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
mr_get_method(1514)
mr_get_method(219)
[JJFB_GUEST_EXT] observed ext_base=0x2AEB04
[JJFB_GUEST_EXT] ordinal=1 role=mrc_loader helper=0x2AEB48 P=0x2AC8DC ext_base=0x2AEB04 len=20
[JJFB_MRP_ALIAS] patched ext_base=0x2AEB04 literal=0x2AEBD8 request=cfunction.ext target=robotol.ext method=ext_base_0xD4
_mr_c_function_new(002AEB48, 20)  mr_c_function_P:002AC8DC
mr_get_method(161178)
[JJFB_GUEST_EXT] observed ext_base=0x2D8DF4
[JJFB_ROBOTOL_LOAD] ordinal=3 guest_ordinal=2 helper=0x304AED P=0x2AC8DC ext_base=0x2D8DF4 loader_helper=0x2AEB48 after_alias=1 source=br_log
_mr_c_function_new(00304AED, 20)  mr_c_function_P:002AC8DC
[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x1
bridge_dsm_mr_start_dsm('gwy/jjfb.mrp','start.mr',SRCV): 0x1
[JJFB_START_HANDOFF] raw_ret=0x1 semantic=MR_IGNORE alias_applied=1 robotol_loaded=1 helper=0x304AED action=run_host_801_recovery
[JJFB_801_GUARD] robotol_loaded=1 alias_applied=1 robotol_helper=0x304AED action=run_host_801
[JJFB_801] synced guest EXT helper=0x304AED P=0x2AC8DC ER_RW=0x2B1858 ext=@2D8DF4
[JJFB_801] P@0x2AC8DC = {ER_RW=0x2B1858 len=5404 type=1 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=6 input=0x2D8DF4 len=1968 P=0x2AC8DC erw=0x2B1858 helper=0x304AED
[JJFB_801] ext_call code=6 ret=0 out_len=0
[JJFB_801] host version(6) ret=0
[JJFB_801] P@0x2AC8DC = {ER_RW=0x2B1858 len=5404 type=1 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=8 input=0x2829D4 len=16 P=0x2AC8DC erw=0x2B1858 helper=0x304AED
[JJFB_801] ext_call code=8 ret=0 out_len=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] P@0x2AC8DC = {ER_RW=0x2B1858 len=5404 type=1 chunk=0x0 stack=0x0}
[JJFB_801] installed sendAppEvent hook @0x2829D4
[JJFB_801] synthesized mrc_extChunk @0x2829DC -> P+0xc (send=@0x2829D4 extMrTable=0x281EFC)
[JJFB_801] ext_call code=0 input=0x2D8DF4 len=1968 P=0x2AC8DC erw=0x2B1858 helper=0x304AED
[JJFB_801] ext_call code=0 ret=0 out_len=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer period=50 RUNNING=1
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
```
