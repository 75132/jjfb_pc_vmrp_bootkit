# JJFB-only v9：固定 sdk_key=g:u2 后直接启动机甲风暴

## 这次反馈结论

v8 已经找到关键点：

```text
sdk_key.dat 正确候选 = g:u2
sha1 = c0d64c43aa023a14a4546288fab5d07f9ca9b9d9
```

v7 的错误是：

```text
start.mr:143: cann`t find sdk key!
```

v8 第一个候选 `g:u2` 后，stdout 里不再出现这个错误，只剩：

```text
libpng warning: iCCP: known incorrect sRGB profile
```

所以现在不要继续探测 sdk_key。下一步就是固定 `sdk_key.dat=g:u2`，直接长时间启动 jjfb，观察是否进入机甲风暴界面 / loading / 网络连接。

## 使用

解压到：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
```

覆盖。

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BOOT_JJFB_FIXED_KEY_V9.ps1
```

不需要管理员。

## 它会做什么

```text
1. 把 gwy/jjfb.mrp 复制成 mythroad/dsm_gm.mrp
2. 把 sdk_key.dat 写成 g:u2
3. 写入多个位置：
   mythroad/sdk_key.dat
   mythroad/gwy/sdk_key.dat
   mythroad/gwy/jjfbol/sdk_key.dat
   mythroad/240x320/sdk_key.dat
   mythroad/240x320/gwy/sdk_key.dat
4. 启动 vmrp
5. 等 120 秒
6. 记录 stdout / netstat / tasklist
7. 打包 jjfb_fixed_key_feedback_*.zip
```

## 你要观察

这次重点看窗口，不是 PowerShell：

```text
是否出现机甲风暴标题
是否出现 loading
是否出现正在登录/连接服务器
是否黑屏但无 sdk_key 报错
是否有 netstat 到 20000
```

跑完发：

```text
logs\jjfb_fixed_key_feedback_*.zip
```
