# 最新路线：v53 Start Handoff Recovery

## 已锁定事实

- 目标不是复刻 UI，而是让原始 `gwy/jjfb.mrp` 按平台启动链自然进入游戏模块。
- v51 已解决 canonical `sdk_key.dat`。
- v52 本机实测已解决 MRP 内别名：`cfunction.ext → robotol.ext`。
- 正确 hook 不在 outer `br__mr_c_function_new`，而在 guest DSM 的 `br_log`：观察 `--- ext:` 和 `_mr_c_function_new(...)` 后改写 `ext_base+0xD4`。
- 原始链已达到：`1514 → 219 → 161178 → robotol helper`。
- 当前 raw `bridge_dsm_mr_start_dsm ret=0x1`；host 801 尚未执行。

## v53 唯一任务

只恢复 robotol 装载完成后的平台交接：

```text
raw start_dsm = MR_IGNORE(1)
+ alias_applied
+ robotol_loaded
→ host version(6)
→ host appInfo(8)
→ host mrc_init(0)
```

这不是全局吞掉返回值。任何缺少 alias/robotol 后置条件的 `ret=1` 都必须停止在 host 801 之前。

## 禁止回退

- 不回 UI、动画、AC8、progress gate。
- 不覆盖 canonical `jjfb.mrp`。
- 不再用 patched `dsm_gm.mrp` 作为正式入口。
- 不伪造外部 `cfunction.ext` 文件。
