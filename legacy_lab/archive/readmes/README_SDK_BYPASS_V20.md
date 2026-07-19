# JJFB v20：真正绕过 sdk_key 错误调用

## v19 结果

v19 已经修好了 MRP 重打包问题：

```text
mr_get_method(1502)
mr_unzip
```

但新的错误是：

```text
start.mr:143: bad argument #1 to `_gc' (number expected, got string)
```

这说明 v19 把：

```text
_error("cann`t find sdk key!")
```

改成了：

```text
_gc("cann`t find sdk key!")
```

但是 `_gc` 的第 1 个参数必须是数字，不能是字符串。

## v20 修正

v20 会同时改两处：

```text
1. 把函数名常量：
   _error  ->  _gc

2. 把参数常量：
   "cann`t find sdk key!"  ->  数字 0
```

这样实际执行变成：

```text
_gc(0)
```

它应该不会再因为 sdk_key 错误停住。

同时 v20 会同步更新 MRP 文件表里的 start.mr 压缩大小，避免 v18 的白屏问题。

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_JJFB_SDK_BYPASS_V20.ps1
```

## 这次成功标志

不应该再出现：

```text
cann`t find sdk key!
bad argument #1 to `_gc'
cannot read start.mr
```

下一步可能会出现：

```text
mrc_loader.ext
robotol
_mr_c_load
连接服务器
新的 start.mr 行号错误
黑屏但无 sdk 错误
```

跑完发：

```text
logs\sdk_bypass_v20_feedback_*.zip
```

以及 vmrp 窗口截图。
