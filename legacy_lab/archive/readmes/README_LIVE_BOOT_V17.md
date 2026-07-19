# JJFB v17：实时启动观察版，不再盲测参数

## v16 结论

v16 说明：

```text
第 4 参数补丁确实成功；
多个参数候选都没有产生网络连接；
stdout 只有 9 条 PNG warning。
```

这里要注意：v16 是把 stdout 重定向到文件并且 25 秒后 kill。  
如果 jjfb 进入界面后进程一直活着，stdout 可能没有及时 flush，所以日志不完整。

现在不能继续只靠 zip 盲猜参数了，必须看 PC 模拟器窗口到底停在哪里。

## v17 做什么

v17 固定使用目前最可信的 cfg 参数：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

然后：

```text
1. main.exe 第 4 参数继续补成上述 param；
2. sdk_key.dat = g:u2；
3. dsm_gm.mrp = jjfb.mrp；
4. 启动 main.exe；
5. 不 kill 进程；
6. 让窗口一直开着；
7. 后台每 10 秒记录 netstat，持续 5 分钟；
8. 生成 live_boot_feedback_*.zip。
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_LIVE_BOOT_JJFB_V17.ps1
```

## 你要做的事

脚本启动后，重点看模拟器窗口：

```text
1. 是黑屏？
2. 是机甲风暴标题？
3. 是 loading？
4. 是角色/登录界面？
5. 是卡在某个图片？
6. 能不能按 Enter / 方向键 / 数字键 5 继续？
```

请截图窗口一起发回来，并发：

```text
logs\live_boot_feedback_*.zip
```

## 如果要关闭

手动关闭 vmrp 窗口即可。

## 恢复 main.exe

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_MAIN_EXE_V17.ps1
```
