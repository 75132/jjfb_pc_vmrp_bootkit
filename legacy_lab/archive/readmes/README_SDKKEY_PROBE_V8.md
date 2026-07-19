# JJFB-only v8：sdk_key 探测版

## 当前日志结论

v7 已经证明：

```text
vmrp 确实直接启动了 jjfb.mrp
mr_open(mythroad/dsm_gm.mrp,1)
mr_open(mythroad/sdk_key.dat,1)
start.mr:143: cann`t find sdk key!
```

这很关键：不是找不到文件路径。

它已经打开了：

```text
mythroad/sdk_key.dat
```

但仍然报：

```text
cann`t find sdk key!
```

所以问题是：

```text
sdk_key.dat 内容不对，不是路径不存在。
```

v7 写进去的是假的：

```text
123456789012345
```

这个只能证明路径通了，不能满足 jjfb 的 start.mr 校验。

## v8 做什么

v8 不再固定写假的 sdk_key，而是自动尝试多组候选：

```text
1. 保留/读取现有 game_files 里的 sdk_key.dat 候选
2. 从 cfg.bin index 36 的机甲风暴记录里提取字段
3. 尝试 raw 二进制字段 / hex 字符串字段 / key=value 字段
4. 尝试常见文本格式：
   sdk_key=...
   key=...
   IMEI=...
   hstype=android
5. 每个候选都写到多个可能位置：
   mythroad/sdk_key.dat
   mythroad/gwy/sdk_key.dat
   mythroad/gwy/jjfbol/sdk_key.dat
   mythroad/240x320/sdk_key.dat
   mythroad/240x320/gwy/sdk_key.dat
6. 每个候选启动一次 jjfb
7. 如果日志不再出现 cann`t find sdk key，就停止并保留现场
```

## 用法

解压到：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
```

覆盖。

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_JJFB_SDKKEY_PROBE_V8.ps1
```

跑完发：

```text
logs\sdkkey_probe_feedback_*.zip
```

## 结果怎么看

- 如果某个 candidate 后不再出现 `cann`t find sdk key!`，说明 sdk_key 过了，下一关才是 mrc_loader/robotol/_mr_param。
- 如果所有 candidate 都报 sdk_key，说明必须从原始手机/冒泡安装环境里提取真实 sdk_key.dat，或者继续反 start.mr 的 key 校验逻辑。
