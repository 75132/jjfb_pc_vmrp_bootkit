# v52 MRP Member Alias 运行结果

- 日志：`/mnt/data/jjfb_v52_work/sim/success.log`
- 总行数：12
- FILEOPEN_MISS：0
- 原始 MRP SHA256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- SDK key SHA256：`未提供`

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
- 801 guard 放行：是
- 801 guard 阻止 loader 误判：否
- host `mrc_init(0)`：0
- robotol timer RUNNING：是

## 2. 解包与 helper 顺序

- `mr_get_method`：`[1514, 219, 161178]`
- `_mr_c_function_new`：`[('1', '000A4088', '20'), ('2', '002AEB48', '20'), ('3', '00304AE5', '20')]`

## 3. 自动结论

- start.mr → mrc_loader(219) → alias → robotol(161178) → mrc_init=0 链已恢复。

## 4. 关键日志

```text
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
[JJFB_LOADER] _mr_c_function_new #1 helper=000A4088 len=20
mr_get_method(219)
[JJFB_LOADER] _mr_c_function_new #2 helper=002AEB48 len=20
[JJFB_MRP_ALIAS] patched helper=0x2AEB48 literal=0x2AEC14 request=cfunction.ext target=robotol.ext method=known_offset
mr_get_method(161178)
[JJFB_LOADER] _mr_c_function_new #3 helper=00304AE5 len=20
[JJFB_ROBOTOL_LOAD] ordinal=3 helper=0x304AE5 loader_helper=0x2AEB48 after_alias=1
[JJFB_801_GUARD] robotol_loaded=1 alias_applied=1 current_helper=0x304AE5 action=run_host_801
[JJFB_801] host mrc_init(0) ret=0
ARM robotol timer RUNNING=1
```
