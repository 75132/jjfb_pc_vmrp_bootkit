# v52 MRP 内成员别名静态审计

- MRP：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy\jjfb.mrp`
- SHA-256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- 文件大小：414602 字节
- 索引成员：50

## 1. 原始成员事实

- `start.mr` 压缩长度：1514
- `mrc_loader.ext` 压缩长度：219
- `mrc_loader.ext` 解压长度：232
- `robotol.ext`：offset=231594，压缩长度=161178
- `cfunction.ext` 成员存在：否

## 2. v52 别名落点

- loader 中 `cfunction.ext\0` 偏移：`0xD4`
- `mr_c_function_load` 入口偏移：`0x8`
- helper 到请求字面量的增量：`0xCC`

因此 host 在第二阶段 `_mr_c_function_new` 回调中，仅把 guest 内存中的请求字面量：

```text
cfunction.ext -> robotol.ext
```

原始 `jjfb.mrp` 文件和索引均不修改。

## 3. 严格检查

- required_members: 通过
- cfunction_absent: 通过
- canonical_lengths: 通过
- loader_decompressed_232: 通过
- literal_at_0xD4: 通过
- helper_delta_0xCC: 通过

**总判定：通过**
