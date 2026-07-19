# JJFB v23：跳过 line157 `_mr_c_load`，确认下一层

## v22 结果

v22 已经证明：

```text
sdk_key 错误分支已经成功跳过
程序进入 line157
```

当前新错误是：

```text
start.mr:157: err:call global `_mr_c_load' (object)
```

这说明现在卡点已经不是 sdk_key，而是 `start.mr` 正常流程里的 C/EXT loader。

## 这个错误是什么意思

`_mr_c_load` 在当前 PC vmrp 里不是普通 Lua 函数，而是 object。  
所以直接调用它会报：

```text
call global `_mr_c_load' (object)
```

这很像当前 vmrp Windows 版对 MRP 的 C 扩展 loader 支持不完整。

## v23 做什么

v23 不再乱替换 `_mr_c_load`，而是做一次控制流探测：

```text
1. 保留 v22 的 sdk 错误分支跳过；
2. 额外把 line157 的第一条指令改成 JMP；
3. 直接跳到 line157 后面的下一行；
4. 看下一层错误是什么。
```

如果跳过 line157 后能继续到新的行号/新错误，说明还有文件层面推进空间。  
如果跳过后仍然无法进入主逻辑，基本可以确认：`_mr_c_load` 是必经 loader，必须改 vmrp 模拟器本身，而不是继续 patch jjfb.mrp。

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SKIP_LINE157_V23.ps1
```

跑完发：

```text
logs\skip_line157_v23_feedback_*.zip
```

以及 vmrp 窗口截图。
