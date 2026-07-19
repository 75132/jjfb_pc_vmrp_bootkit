# v51 Valid SDK Key 运行结果

- 日志：`logs\v51_valid_sdk_key_stdout.txt`
- 总行数：163
- FILEOPEN_MISS：0
- 部署 key：48 字节，SHA256 `5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436`

## 1. SDK key 证据

- `cann`t find sdk key!`：否
- canonical `start.mr` 压缩长度 1514：是
- `mrc_loader.ext` 解包 217/219：是（实际=219）
- `robotol.ext` 解包 161178：否
- `cfunction.ext` 读取失败 code=3006：是
- robotol helper 日志：否
- `bridge_dsm_mr_start_dsm ret=0x0`：是
- host `mrc_init(0)`：0
- robotol timer RUNNING：是

实际打开的 sdk_key.dat host：

```text
C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/sdk_key.dat
```

部署 key 十六进制：

```text
d6aaa1b23878829303d1b9bcca42e1839f9b63153641f4c4e9743434427125b29b31f52a08537f4bdd1b71ab70686d35
```

## 2. MRP 打开与解包顺序

`mr_get_method`：`[1514, 219]`

MRP host 路径：

```text
C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp
C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp
C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp
C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp
```

## 3. 自动结论

- sdk key 已通过，mrc_loader.ext 已装载；二阶段在请求 `cfunction.ext` 时失败（MRP 内实际名为 `robotol.ext`）。

## 4. 关键日志

```text
﻿[JJFB_SDK_KEY] vmver=1968 IMEI=864086040622841 hsman=vmrp hstype=vmrp
[JJFB_SDK_KEY] canonical=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\sdk_key.dat len=48 sha256=5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436
ext call _mr_c_function_new(0xA4088[671880], 0x14[20])
mr_get_method(1514)
mr_get_method(219)
_mr_c_function_new(002AEB48, 20)  mr_c_function_P:002AC8DC
read file  "cfunction.ext" err, code=3006
[DEBUG]mr_exit() called by mythroad!
[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x0
[JJFB_801] synced guest EXT helper=0x2AEB48 P=0x2AC8DC ER_RW=0x0 ext=@2AEB04
[JJFB_801] P@0x2AC8DC = {ER_RW=0x0 len=0 type=0 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=6 input=0x2AEB04 len=1968 P=0x2AC8DC erw=0x0 helper=0x2AEB48
[JJFB_801] ext_call code=6 ret=0 out_len=0
[JJFB_801] host version(6) ret=0
[JJFB_801] P@0x2AC8DC = {ER_RW=0x0 len=0 type=0 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=8 input=0x2829D4 len=16 P=0x2AC8DC erw=0x0 helper=0x2AEB48
[JJFB_801] ext_call code=8 ret=0 out_len=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] P@0x2AC8DC = {ER_RW=0x0 len=0 type=0 chunk=0x0 stack=0x0}
[JJFB_801] installed sendAppEvent hook @0x2829D4
[JJFB_801] synthesized mrc_extChunk @0x2829DC -> P+0xc (send=@0x2829D4 extMrTable=0x281EFC)
[JJFB_801] ext_call code=0 input=0x2AEB04 len=1968 P=0x2AC8DC erw=0x0 helper=0x2AEB48
[JJFB_801] ext_call code=0 ret=0 out_len=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer period=50 RUNNING=1
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
read file  "cfunction.ext" err, code=3006
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
[JJFB_801] ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x0 helper=0x2AEB48 (timer)
[JJFB_801] ext_call code=2 ret=0 out_len=0
```
