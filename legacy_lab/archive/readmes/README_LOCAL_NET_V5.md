# JJFB PC vmrp local network v5：PC端已经启动到联网，下一步把 111.1.17.148 / 211.155.236.226 劫到本地 mock

## 这次日志结论

这次不是失败，是大进展。

`RUN_PC_BOOT_GWY_V4` 已经让 vmrp 直接启动了 `gwy`，并且日志里出现了真实联网动作：

```text
mr_initNetwork(mod:cmnet)
mr_socket(type:0, protocol:0)
mr_connect(s:1, ip:1862341012, port:21002, type:1)
my_connect('111.1.17.148', 21002)
```

同时 netstat 里还能看到：

```text
192.168.31.44:* -> 211.155.236.226:20000 SYN_SENT
```

说明 PC 端路线已经走到网络层了。

之前 mock 没收到包，不是 mock 错，而是 vmrp 直接连了公网 IP 字面量：

```text
111.1.17.148:21002
211.155.236.226:20000
```

hosts 对 IP 字面量无效，所以必须把这些 IP 变成本机 IP alias，让连接落到本地 mock。

## 本包做什么

这个 v5 会：

```text
1. 保持 v4 的 dsm_gm.mrp -> gwy 直接替换；
2. 启动本地 mock：21002 / 21003 / 20000 / 6009；
3. 以管理员权限给当前网卡临时添加本地 IP alias：
   - 111.1.17.148
   - 111.1.17.146
   - 211.155.236.226
4. 这样 vmrp 连接这些 IP 时会打到本机 mock；
5. 启动 vmrp 并收集日志。
```

## 必须管理员运行

右键 PowerShell → 以管理员身份运行，然后：

```powershell
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
powershell -ExecutionPolicy Bypass -File .\RUN_PC_GWY_LOCAL_NET_V5_ADMIN.ps1
```

跑完发：

```text
logs/localnet_feedback_*.zip
```

## 恢复网络别名

测试完可以运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_RESTORE_LOCAL_NET_V5_ADMIN.ps1
```

## 成功标志

最关键看 mock 是否收到：

```text
mock_localnet_*.jsonl
```

里面如果出现：

```text
port 21002 recv LOGIN_103B
port 20000 recv ...
```

PC 端本地 mock 就正式接上了。
