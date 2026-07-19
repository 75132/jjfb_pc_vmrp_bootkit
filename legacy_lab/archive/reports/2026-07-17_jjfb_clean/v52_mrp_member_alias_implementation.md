# v52 MRP 内成员别名实施报告

## 1. 本轮目标

沿 v51 已确认的原始启动链继续推进，不返回 UI，不修改 `gwy/jjfb.mrp`：

```text
start.mr (1514)
→ mrc_loader.ext (219)
→ 请求 cfunction.ext
→ MRP 内不存在该成员
→ err code=3006 / mr_exit
```

v52 只在 host 执行环境中，把第二阶段 loader 的成员请求解释为：

```text
cfunction.ext → robotol.ext
```

## 2. 静态证据

对 canonical `gwy/jjfb.mrp` 的严格审计结果：

- 文件大小：414602 字节；
- SHA-256：`52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`；
- `start.mr` 压缩长度：1514；
- `mrc_loader.ext` 压缩长度：219，解压长度：232；
- `robotol.ext` 偏移：231594，压缩长度：161178；
- MRP 索引中不存在 `cfunction.ext`；
- `mrc_loader.ext` 内 `cfunction.ext\0` 位于文件偏移 `0xD4`；
- EXT 入口为文件偏移 `0x8`，因此请求字面量位于 `helper + 0xCC`。

严格审计的六项检查全部通过，详见 `v52_mrp_member_alias_static_audit.md`。

## 3. Host 实现

### 3.1 Guest 内存别名

在第二次 `_mr_c_function_new` 回调，即已确认的 `mrc_loader.ext` helper 注册阶段：

1. 首先检查 `helper + 0xCC` 是否恰好为完整的 `cfunction.ext\0`；
2. 命中后写入等长缓冲区 `robotol.ext\0\0\0`；
3. 若 canonical 偏移未命中，仅在 helper 邻近范围查找完整、NUL 终止的同名字面量；
4. 不修改 MRP 文件、不修改 MRP 索引，也不创建外部伪造的 `cfunction.ext`。

别名只在环境变量 `JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL=1` 时启用。

为避免再次误认 helper，v52 使用保守顺序：

```text
#1 平台 EXT
#2 mrc_loader.ext：应用别名
#3 robotol.ext：记录为真正 robotol helper
```

若第二或第三个 helper 顺序不符合预期，状态不会被标记为 robotol 成功。

### 3.2 801 误判保护

v51 在 `mrc_loader.ext` 停止后，`mr_extHelper_addr` 仍指向 loader，host 随后执行 `6 → 8 → 0`，可能得到 loader 的 `ret=0` 并误开 timer。

v52 增加 `JJFB_801_GUARD`：

- 未出现 alias 后第三个新 helper：跳过 host 801；
- 只有确认新 robotol helper 注册后，才执行 `version(6) → appInfo(8) → mrc_init(0)`；
- 因此 `mrc_init=0` 不再能由 mrc_loader 伪装成 robotol 成功。

## 4. 新增日志判据

成功链应包含：

```text
[JJFB_MRP_ALIAS] enabled=1 request=cfunction.ext target=robotol.ext
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] patched ... method=known_offset
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ordinal=3 ... after_alias=1
[JJFB_801_GUARD] robotol_loaded=1 ... action=run_host_801
[JJFB_801] host mrc_init(0) ret=...
```

失败保护链应包含：

```text
[JJFB_801_GUARD] robotol_loaded=0 ... action=skip_host_801
```

此时不应出现由 loader 产生的伪 `mrc_init=0` 或 robotol timer RUNNING。

## 5. 已完成验证

- Python 三个脚本均通过 `py_compile`；
- canonical MRP 严格审计通过；
- 48 字节 SDK key 重新生成后逐字节一致；
- SDK key SHA-256：`5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436`；
- guest 内存偏移模拟确认 `helper + 0xCC` 对应 `cfunction.ext\0`；
- 别名写入保持 14 字节槽位，不移动 loader 代码或数据；
- 成功模拟日志正确判定完整链恢复；
- 失败模拟日志正确判定 801 guard 阻止误调用；
- `bridge.c`、`vmrp.c` 均通过 C 语法检查；
- 增量 unified diff 已在原始三份源文件上 dry-run 和实际应用通过。

Linux 64 位语法检查仍显示原工程已有的 32/64 位指针转换警告；v52 新增代码未引入编译错误。最终 Windows 32 位运行结果仍以本机 `RUN_V52_MRP_MEMBER_ALIAS.ps1` 输出为准。

## 6. 本机运行

覆盖到原项目根目录后运行：

```powershell
.\RUN_V52_MRP_MEMBER_ALIAS.ps1 -Seconds 25
```

主要输出：

```text
logs\v52_mrp_member_alias_stdout.txt
reports\v52_mrp_member_alias_static_audit.md
reports\v52_mrp_member_alias_run_result.md
```

下一步判据：

- 出现 `161178` 且 robotol helper 注册：别名阶段完成，转入 robotol `_strCom 800/801`；
- 出现 `161178` 但无第三个 helper：审计 robotol EXT 注册入口；
- alias 已应用但仍报 3006：检查 loader 在比较成员名之前是否复制了请求字符串；
- alias 未应用：检查是否实际编译并运行含 v52 标记的 `main.exe`。
