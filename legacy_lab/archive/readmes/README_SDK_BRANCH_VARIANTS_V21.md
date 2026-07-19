# JJFB v21：sdk 错误分支多方案测试

## v20 结果

v20 已经成功做到：

```text
cannot read start.mr：已解决
cann`t find sdk key：已绕过
bad argument #1 to `_gc'：已解决
bridge_dsm_mr_start_dsm(..., S20P): 0x0
```

但窗口仍然白屏，且没有网络。

这说明 v20 很可能只是把原来的：

```text
return _error("cann`t find sdk key!")
```

改成了：

```text
return _gc(0)
```

也就是不报错了，但仍然从 sdk 错误分支直接返回，没有进入后面的 `mrc_loader.ext` 加载流程。

## v21 做什么

v21 不再只把错误分支变成 `_gc(0)`，而是测试几种“把错误分支导向 loader”的方案：

```text
A. _error("cann`t find sdk key!") -> _mr_c_load("mrc_loader.ext")
B. _error("cann`t find sdk key!") -> _mr_c_buf("mrc_loader.ext")
C. _error("cann`t find sdk key!") -> _com("mrc_loader.ext")
D. _error("cann`t find sdk key!") -> _strCom("mrc_loader.ext")
E. baseline _gc(0)
```

每个方案都会：

```text
1. patch jjfb.mrp/start.mr；
2. 同步更新 MRP 文件表 start.mr 压缩大小；
3. dsm_gm.mrp = patched jjfb；
4. main.exe 第 4 参数继续注入 cfg 参数；
5. 启动 20 秒；
6. 收集 stdout/netstat；
7. 出现 mrc_loader.ext / mr_socket / connect / 20000 / 21002 时停止。
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SDK_BRANCH_VARIANTS_V21.ps1
```

跑完发：

```text
logs\sdk_branch_variants_feedback_*.zip
```

以及 vmrp 窗口截图。
