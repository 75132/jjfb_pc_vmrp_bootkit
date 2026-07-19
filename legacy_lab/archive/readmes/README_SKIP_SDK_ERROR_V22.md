# JJFB v22：不再替换函数名，直接跳过 sdk 错误分支

## v21 结果

v21 说明：把错误分支改成其他函数调用不是正确方向。

截图里已经出现：

```text
call global `_mr_c_buf' (nil)
bad argument #2 to `_strCom'
```

这说明我们还在执行原来的 sdk 错误分支，只是把错误分支里调用的函数名换掉了，所以仍然不会进入真正的游戏加载流程。

## v22 改法

这次不改 `_error`，也不改 `"cann`t find sdk key!"`。

而是直接改 `start.mr` 的字节码控制流：

```text
line 143: _error("cann`t find sdk key!"); return
line 147: _error(""); return
```

v22 会把这两个错误分支的第一条指令改成 JMP，直接跳到 line 157 后面的正常流程：

```text
pc100 line143 -> JMP 到 pc109
pc105 line147 -> JMP 到 pc109
```

也就是：

```text
保留原 start.mr 结构
保留原常量
只绕过 sdk 错误分支
继续执行后面的 mrc_loader / 游戏初始化代码
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SKIP_SDK_ERROR_V22.ps1
```

## 成功标志

这次不应该再出现：

```text
cann`t find sdk key
bad argument
cannot read start.mr
```

如果 patch 正确，下一步应该出现：

```text
mrc_loader.ext
_mr_c_load
_mr_c_buf
_strCom
robotol.ext
connect / 20000 / 21002
```

跑完发：

```text
logs\skip_sdk_error_v22_feedback_*.zip
```

以及 vmrp 窗口截图。
