# v52 MRP Member Alias 运行结果

- 日志：`logs\v52_mrp_member_alias_stdout.txt`
- 总行数：81
- FILEOPEN_MISS：0
- 原始 MRP SHA256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- SDK key SHA256：`5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436`

## 1. 链路判据

- SDK 报错：否
- `start.mr` 1514：是
- `mrc_loader.ext` 219：是
- alias enabled：是
- alias patched：是
- alias patch miss：否
- `err code=3006`：否
- `robotol.ext` 161178：是
- 新 robotol helper 注册：是
- `bridge_dsm_mr_start_dsm` ret：0x1
- 801 guard 放行：否
- 801 guard 阻止 loader 误判：否
- host `mrc_init(0)`：未调用/未捕获
- robotol timer RUNNING：否

## 2. 解包与 helper 顺序

- `mr_get_method`：`[1514, 219, 161178]`
- `_mr_c_function_new`：`[('1', '000a4088', '20')]`

## 3. 自动结论

- 别名阶段完成（161178 + ROBOTOL_LOAD），但 start_dsm ret=0x1；下一轮审计 robotol `_strCom 800/801` 返回值（host 801 因 start_dsm 失败未执行）。

## 4. 关键日志

```text
[JJFB_MRP_ALIAS] contract=cfunction.ext->robotol.ext scope=guest_memory_only
[JJFB_LOADER] _mr_c_function_new #1 helper=000a4088 len=20
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] patched helper=0x2AEB0C literal=0x2AEBD8 request=cfunction.ext target=robotol.ext method=ext_base_0xD4
--- r9: mr_c_function_P.start_of_ER_RW = @00000000
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ordinal=3 helper=0x304AED loader_helper=0x2AEB48 after_alias=1
--- r9: mr_c_function_P.start_of_ER_RW = @002B1858
[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x1
```
