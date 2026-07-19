# v52 MRP Member Alias 运行结果

- 日志：`/mnt/data/jjfb_v52_work/sim/guard.log`
- 总行数：9
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
- `err code=3006`：是
- `robotol.ext` 161178：否
- 新 robotol helper 注册：否
- 801 guard 放行：否
- 801 guard 阻止 loader 误判：是
- host `mrc_init(0)`：未调用/未捕获
- robotol timer RUNNING：否

## 2. 解包与 helper 顺序

- `mr_get_method`：`[1514, 219]`
- `_mr_c_function_new`：`[('1', '000A4088', '20'), ('2', '002AEB48', '20')]`

## 3. 自动结论

- 别名已写入，但尚未解出 161178；继续看 3006、mr_exit 和 loader 调用顺序。

## 4. 关键日志

```text
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
[JJFB_LOADER] _mr_c_function_new #1 helper=000A4088 len=20
mr_get_method(219)
[JJFB_LOADER] _mr_c_function_new #2 helper=002AEB48 len=20
[JJFB_MRP_ALIAS] patched helper=0x2AEB48 literal=0x2AEC14 request=cfunction.ext target=robotol.ext method=known_offset
err code=3006
mr_exit
[JJFB_801_GUARD] robotol_loaded=0 alias_applied=1 current_helper=0x2AEB48 action=skip_host_801
```
