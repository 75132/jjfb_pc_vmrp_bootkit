# v53 Start Handoff Recovery 实施报告

## 1. 起点

v52 经本机补 hook 后已实测达到：

```text
start.mr(1514)
→ mrc_loader.ext(219)
→ cfunction.ext 字面量在 ext_base+0xD4 改为 robotol.ext
→ robotol.ext(161178)
→ robotol helper=0x304AED
→ start_dsm ret=0x1
```

`jjfb.mrp` SHA-256 保持不变。

## 2. v52 hook 修正正式并入 v53

原 v52 误以为 guest EXT 注册会经过 outer `br__mr_c_function_new`。实际 mrc_loader/robotol 走 guest 内 `_mr_c_function_table`。

v53 将检测放入 `br_log`：

1. 捕获 `--- ext: @XXXXXXXX`，保存当前 EXT base；
2. 捕获 `_mr_c_function_new(helper,len) mr_c_function_P:P`；
3. 第一个 guest helper 视为 mrc_loader，在 `ext_base+0xD4` 精确改写请求字面量；
4. 下一不同 helper 且 alias 已应用时，标记 robotol 已注册。

## 3. MR_IGNORE 后置恢复

`vmrp.c` 新增环境门：

```text
JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL=1
```

逻辑：

```c
start_handoff_ready = (ret == MR_SUCCESS);

if (ret == MR_IGNORE
    && alias mode
    && alias applied
    && robotol loaded) {
    start_handoff_ready = 1;
}
```

随后复用已存在的严格 801 guard，并执行：

```text
bridge_dsm_ext_call(6, version)
bridge_dsm_ext_call(8, appInfo)
bridge_dsm_ext_call(0, init)
```

只有 `mrc_init(0)==0` 才启动 robotol timer。

## 4. 不做的事

- 不把所有 `start_dsm=1` 全局改成成功；
- 不改 `start.mr`、`mrc_loader.ext`、`robotol.ext`；
- 不修改 MRP 索引或 SHA-256；
- 不回 UI gate；
- 不把 mrc_loader 的 `ret=0` 冒充 robotol 初始化。

## 5. 验证

### 静态 MRP 审计

- canonical MRP SHA-256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- `start.mr=1514`
- `mrc_loader.ext=219`，解压 232
- `robotol.ext=161178`
- `cfunction.ext` 不存在
- 请求字面量在 loader `0xD4`

### C 语法检查

使用完整源码树叠加 v50→v51→v52→v53 后执行：

```text
gcc -fsyntax-only -g -Wall -DNETWORK_SUPPORT -DVMRP -D_WIN32 \
  -I./windows/unicorn-1.0.2-win32/include bridge.c vmrp.c
```

退出码 0。现有 15 条警告均为旧工程在 64 位 Linux 检查时的指针宽度或未使用变量警告；未出现 v53 语法错误。

### 分析器模拟

- 成功链：能识别 `raw_ret=1 → recovery → 6/8/0=0 → timer RUNNING`。
- 守卫链：恢复环境门关闭时，能识别 `stop_before_host_801`，不会误报成功。

## 6. 尚未宣称的结果

当前环境不能执行 Windows/MSYS2 32 位程序，因此本报告没有宣称 v53 已在本机动态跑通。最终判定以 Windows 运行生成的：

```text
reports/v53_start_handoff_run_result.md
```

为准。
