# v53 Start Handoff Recovery 静态审计

- MRP：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\320x480\gwy\jjfb.mrp`
- SHA-256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- 文件大小：414602 字节
- 索引成员：50

## 1. Canonical MRP 事实

- `start.mr` 压缩长度：1514
- `mrc_loader.ext` 压缩/解压长度：219 / 232
- `robotol.ext` 压缩长度：161178
- `cfunction.ext` 成员存在：否
- loader 请求字面量偏移：`0xD4`

## 2. v53 Host 契约

- 别名触发点：guest `br_log` 观察到 mrc_loader 的 `--- ext:` 与 `_mr_c_function_new(...)`。
- 内存改写点：`ext_base + 0xD4`，仅把 `cfunction.ext` 改为 `robotol.ext`。
- `start_dsm` 原始返回仍保留；只有 `MR_IGNORE(1) + alias_applied + robotol_loaded` 同时成立，才执行 host `6 → 8 → 0` 恢复。
- 不修改 `jjfb.mrp` 文件或索引。

## 3. 严格检查

- required_members: 通过
- cfunction_absent: 通过
- canonical_lengths_1514_219_161178: 通过
- loader_decompressed_232: 通过
- loader_literal_at_ext_base_0xD4: 通过
- br_log_guest_hook_present: 通过
- robotol_postcondition_exports_present: 通过
- mr_ignore_recovery_is_gated: 通过
- host_contract_order_6_8_0: 通过

**总判定：通过**
