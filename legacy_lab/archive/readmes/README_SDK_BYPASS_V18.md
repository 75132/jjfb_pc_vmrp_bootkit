# JJFB v18：补丁 start.mr，绕过 sdk_key 检查，再启动 JJFB

## 这次截图结论

v17 已经证明：之前把 `sdk_key.dat = g:u2` 的判断是误判。

窗口仍然显示：

```text
start.mr:143
cann`t find sdk key!
```

这说明：

```text
g:u2 不是最终 sdk_key.dat 内容；
它只是 cfg.bin 里 jjfb 记录的字段之一。
```

现在正确路线不是继续改参数，而是先过掉 `start.mr` 的 sdk 检查。

## v18 做什么

v18 会直接 patch `jjfb.mrp` 内部的 `start.mr`：

```text
1. 找 jjfb.mrp 里的 start.mr gzip 数据；
2. 解压 start.mr；
3. 找到常量 `_error`；
4. 把 `_error("cann`t find sdk key!")` 改成 `_gc("cann`t find sdk key!")`；
5. 重新 gzip 压回 jjfb.mrp；
6. 生成 patched jjfb；
7. 把 patched jjfb 复制成 dsm_gm.mrp；
8. 继续写 sdk_key.dat=g:u2；
9. 同时给 main.exe 注入目前最可信参数：
   napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
10. 启动 vmrp，实时观察。
```

这一步不是为了连官方服务器，只是为了本地模拟器里继续启动流程，看下一关卡在哪里。

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_JJFB_SDK_BYPASS_LIVE_V18.ps1
```

## 观察

这次如果成功，窗口应该不再停在：

```text
cann`t find sdk key!
```

可能会进入：

```text
机甲风暴标题 / loading / 连接服务器 / 下一个缺文件或缺参数错误
```

跑完发：

```text
logs\sdk_bypass_feedback_*.zip
```

并截图 vmrp 窗口。

## 恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_V18.ps1
```
