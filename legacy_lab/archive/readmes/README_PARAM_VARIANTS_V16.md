# JJFB v16：真实 cfg 参数候选探测

## v15 结果

v15 的补丁已经真正写进 main.exe 了：

```text
patched_arg at 0x2246
old = c744240c00000000
new = c744240ce8d64000
```

这说明第 4 参数已经从 NULL 改成了 startParam 指针。

但运行结果仍然没有推进：

```text
stdout 仍只有 9 条 libpng warning
netstat 没有 20000 / 21002
```

所以现在不是“参数没注入”，而是“参数内容可能不对”。

## 关键修正

从 cfg.bin 机甲风暴记录看，jjfb 这一项前面有：

```text
0c g:u2 ... 01 e2 ... 02 00 ... 00 01 gwy/jjfb.mrp
```

这里更像：

```text
napptype = 12
nextid   = 482
ncode    = 512
narg     = 0
narg1    = 1
nmrpname = gwy/jjfb.mrp
sdk_key  = g:u2
```

前面 v15 用的是：

```text
napptype=0
```

所以 v16 第一优先测试：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

## 使用

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_PARAM_VARIANTS_V16.ps1
```

跑完发：

```text
logs\param_variants_feedback_*.zip
```

## 观察

脚本会连续测试多个参数候选。窗口可能会多次打开/关闭。

重点看：

```text
是否出现机甲风暴标题
是否出现 loading 后继续
是否出现登录/连接服务器
是否 netstat 出现 20000/21002
```
