# JJFB v19：修复 v18 白屏/ cannot read start.mr 的 MRP 重打包问题

## v18 结果

v18 的思路对了一半：已经成功 patch 到 start.mr 数据区。

但 v18 的重打包方式有问题：

```text
原 start.mr 压缩大小：1514
新 gzip 实际大小：1504
v18 做法：把 1504 后面补 0 到 1514
```

这会导致 vmrp/MRP loader 按 1514 字节读 gzip 时，最后 8 字节 footer 不是 gzip 的真实 CRC/ISIZE，而是补零区域，所以报：

```text
mr_get_method(1514)
mr_malloc invalid memory request
cannot read start.mr
init failed
```

所以 v18 的白屏不是 bypass 逻辑失败，而是 MRP 内部文件 size 没同步更新。

## v19 修正

v19 不再补 0 伪装大小，而是：

```text
1. 解压 jjfb.mrp 里的 start.mr；
2. 把 _error 常量改成有效的 _gc 常量；
3. 重新 gzip；
4. 把 MRP 文件表里 start.mr 的压缩大小从 1514 改成新 gzip 的真实大小；
5. 清空旧 gzip 尾部残留；
6. 再复制成 dsm_gm.mrp 启动。
```

这样 vmrp 会显示类似：

```text
mr_get_method(150x)
```

而不是 1514，并且 gzip footer 会在正确位置。

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_JJFB_SDK_BYPASS_V19.ps1
```

## 观察

这次如果修复成功，窗口不应该再出现：

```text
cannot read start.mr
```

也不应该停在：

```text
cann`t find sdk key!
```

可能出现下一关：

```text
mrc_loader.ext
robotol
连接服务器
loading
黑屏但无 start.mr 错误
```

跑完发：

```text
logs\sdk_bypass_v19_feedback_*.zip
```

以及 vmrp 窗口截图。
