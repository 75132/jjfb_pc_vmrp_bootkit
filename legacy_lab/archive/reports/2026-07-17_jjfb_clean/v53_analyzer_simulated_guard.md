# v53 Start Handoff Recovery 运行结果

- 日志：`/mnt/data/v53work/v53_sim_guard.log`
- 总行数：8
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
- MR_IGNORE 恢复门：否
- stop_before_host_801：是
- 801 guard 放行：否
- 801 guard 阻止误判：否
- host version(6)：未调用/未捕获
- host appInfo(8)：未调用/未捕获
- host mrc_init(0)：未调用/未捕获
- robotol timer RUNNING：否
- timer code=2：否

## 3. 顺序

- `mr_get_method`：`[1514, 219, 161178]`

## 4. 自动结论

- 已确认 MR_IGNORE(1)，但恢复门未放行；检查环境变量与三项后置条件。

## 5. 关键日志

```text
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] patched ext_base=0x2AEB40 literal=0x2AEC14 request=cfunction.ext target=robotol.ext method=ext_base_0xD4
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ordinal=3 guest_ordinal=2 helper=0x304AED P=0x2B183C ext_base=0x304AE5 loader_helper=0x2AEB48 after_alias=1 source=br_log
bridge_dsm_mr_start_dsm('gwy/jjfb.mrp','start.mr',SRCV): 0x1
[JJFB_START_HANDOFF] raw_ret=0x1 alias_applied=1 robotol_loaded=1 recovery_enabled=0 action=stop_before_host_801
```
