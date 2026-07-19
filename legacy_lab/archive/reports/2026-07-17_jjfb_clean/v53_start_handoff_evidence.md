# v53 `start_dsm=1` / 801 契约证据

## 1. 当前 canonical 链（用户 Windows 实测）

```text
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] patched ... method=ext_base_0xD4
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ordinal=3 helper=0x304AED
ER_RW=@002B1858
bridge_dsm_mr_start_dsm ret=0x1
```

这证明 `0x1` 出现时 robotol 已完成解包和 helper 注册，不是 loader/别名失败。

## 2. 常量语义

项目 Mythroad 定义：

```text
MR_SUCCESS = 0
MR_FAILED  = -1
MR_IGNORE  = 1
```

因此 raw `0x1` 的语义是 `MR_IGNORE`。

## 3. 历史四组控制实验

旧 v27 实验虽然使用 patched `dsm_gm.mrp`，但能用于判断哪一步改变外层返回：

| 实验 | 内部 code=6 | 内部 code=0 | robotol 161178 | start_dsm |
|---|---:|---:|---:|---:|
| full801 | 执行 | 执行 | 是 | 1 |
| only6_skip0 | 执行 | 跳过 | 是 | 0 |
| skip6_only0 | 跳过 | 执行 | 是 | 1 |
| skip_all_801 | 跳过 | 跳过 | 是 | 0 |

方向性结论：内部 `_strCom(801, "", 0)` 与外层 `MR_IGNORE(1)` 同步出现；code=6 不是决定因素。

## 4. v53 处理原则

不修改 raw 返回，不全局把 1 强制变 0。仅在：

```text
ret == MR_IGNORE
AND alias_applied
AND robotol_loaded
```

时，认为 guest 已完成 EXT 装载但平台初始化输入不完整，转由 host 补：

```text
version(6) → appInfo(8) → mrc_init(0)
```

否则停止，不调用 host 801。
