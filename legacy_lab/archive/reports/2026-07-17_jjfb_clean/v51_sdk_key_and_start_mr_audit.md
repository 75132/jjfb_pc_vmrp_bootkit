# v51：原始 start.mr / SDK Key / canonical 路径审计

## 1. 已锁定结论

v50 的入口、APPID/APPVER 和 `gwy` 资源根修复均成立；当前中断发生在原始 `start.mr` 的 SDK key 校验之后。

但 v49 对照文件 `runtime/.../mythroad/dsm_gm.mrp` **不是原始 MRP 的等价副本**：

| 文件 | SHA-256 |
|---|---|
| 原始 `game_files/mythroad/240x320/gwy/jjfb.mrp` | `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` |
| v49 当前 `dsm_gm.mrp` | `1297c7055be993bd95f0daa5efdaa9b15db519762cce801c4436fa4589abc2d4` |

后者与历史实验文件 `logs/jjfb_skip_sdk_only_v27.mrp`、`logs/jjfb_801diag_only6_skip0_v27.mrp` 完全同哈希。因此 v49 能进入 robotol，是经过历史字节码补丁后的结果，不能证明 `g:u2` 是正确 SDK key。

## 2. 两份 start.mr 的真实差异

两者解压长度均为 3787 字节，原始压缩长度为 1514，v49 文件为 1513；解压后共有 18 个字节不同，落在 4 个指令位置：

1. `_mr_c_load` 子函数 pc36/pc37：v49 丢弃二阶段 `_strCom(800, ...)` 的真实返回值，强制返回 `0`；
2. 顶层 pc100：v49 用 JMP 跳过 key 不匹配时的 `_error`；
3. 顶层 pc105：v49 用 JMP 跳过 key 文件不可用分支的 `_error`；
4. 顶层 pc122：v49 用 JMP 跳过 `_gc()`。

原始字节码的 key 分支逻辑是正常的：`key == this_key` 时跳到 `_mr_c_load()`，不相等才显示 `cann`t find sdk key!` 并返回。

## 3. 原始 SDK key 算法

算法同时存在于原始 `start.mr` 和打包的 Mythroad SDK key 生成器中：

```text
MD5(custom_base64(part1))
|| MD5(custom_base64(part2))
|| MD5(custom_base64(part3))
```

当前 VMRP 源码的 `GetSysInfo` 固定身份为：

```text
vmver = 1968
IMEI  = 864086040622841 + C 字符串结尾 NUL
hsman = vmrp
hstype= vmrp
```

三段输入及结果：

| 段 | custom_base64 输入结果 | MD5 |
|---|---|---|
| 1 | `BIZshdFmhdNmCdasBYiuCdea` | `d6aaa1b23878829303d1b9bcca42e183` |
| 2 | `CYFmhdNmS1roRgroRa==` | `9f9b63153641f4c4e9743434427125b2` |
| 3 | `BdNoBYVqS1ro` | `9b31f52a08537f4bdd1b71ab70686d35` |

因此当前 host 对应的 `sdk_key.dat` 必须是 **48 字节原始二进制**：

```text
d6aaa1b23878829303d1b9bcca42e183
9f9b63153641f4c4e9743434427125b2
9b31f52a08537f4bdd1b71ab70686d35
```

合并后的 SHA-256：

```text
5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436
```

当前运行中的 4 字节 `67 3A 75 32`（ASCII `g:u2`）与算法完全不符。

## 4. v50 路径映射中的次级问题

v50 已让 `gwy/jjfb.mrp` 命中 `mythroad/240x320/gwy/jjfb.mrp`，但 guest 请求是：

```text
mythroad/sdk_key.dat
```

原 v50 resolver 只特殊处理了 `mythroad/gwy/*`，没有处理普通 `mythroad/*`。由于工作目录下存在旧扁平副本：

```text
runtime/.../mythroad/sdk_key.dat
```

它会在 canonical 根之前命中该相对文件。v51 增加规则：

```text
mythroad/<rel> -> JJFB_MYTHROAD_ROOT/<rel>（该 canonical 文件存在时）
```

因此 `mythroad/sdk_key.dat` 会优先解析到：

```text
runtime/.../mythroad/240x320/sdk_key.dat
```

`mythroad/system/gb16.uc2` 若 canonical 根没有对应文件，仍会自然回退到旧的 system 目录，不破坏字体加载。

## 5. v51 的判定目标

运行后应首先观察：

```text
mr_get_method(1514)   # 原始 start.mr
sdk_key.dat host=.../mythroad/240x320/sdk_key.dat
mr_get_method(217)    # mrc_loader.ext
mr_get_method(161178) # robotol.ext
_mr_c_function_new(0030....)
bridge_dsm_mr_start_dsm ret=0x0
[JJFB_801] host mrc_init(0) ret=0
```

若 key 错误消失且出现 217，但没有 161178，则 blocker 已推进到 `mrc_loader.ext` 二阶段；若 161178 出现而 `mrc_init` 非 0，再审计 800/801 契约。v51 不回 UI、不修改原始 `jjfb.mrp`。
