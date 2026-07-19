# v53 Start Handoff Recovery 运行结果

- 日志：`/mnt/data/v53work/v53_sim_success.log`
- 总行数：23
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
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
_mr_c_function_new(002AEB48, 20)  mr_c_function_P:00280400
[JJFB_GUEST_EXT] observed ext_base=0x2AEB40
[JJFB_GUEST_EXT] ordinal=1 role=mrc_loader helper=0x2AEB48 P=0x280400 ext_base=0x2AEB40 len=20
[JJFB_MRP_ALIAS] patched ext_base=0x2AEB40 literal=0x2AEC14 request=cfunction.ext target=robotol.ext method=ext_base_0xD4
mr_get_method(219)
mr_get_method(161178)
_mr_c_function_new(00304AED, 20)  mr_c_function_P:002B183C
[JJFB_GUEST_EXT] observed ext_base=0x304AE5
[JJFB_ROBOTOL_LOAD] ordinal=3 guest_ordinal=2 helper=0x304AED P=0x2B183C ext_base=0x304AE5 loader_helper=0x2AEB48 after_alias=1 source=br_log
[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x1
bridge_dsm_mr_start_dsm('gwy/jjfb.mrp','start.mr',SRCV): 0x1
[JJFB_START_HANDOFF] raw_ret=0x1 semantic=MR_IGNORE alias_applied=1 robotol_loaded=1 helper=0x304AED action=run_host_801_recovery
[JJFB_801_GUARD] robotol_loaded=1 alias_applied=1 robotol_helper=0x304AED action=run_host_801
[JJFB_801] synced guest EXT helper=0x304AED P=0x2B183C ER_RW=0x2B1858 ext=@304AE5
[JJFB_801] host version(6) ret=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer period=50 RUNNING=1
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2B183C erw=0x2B1858 helper=0x304AED (timer)
```
