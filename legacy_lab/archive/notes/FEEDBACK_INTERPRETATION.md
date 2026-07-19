# 反馈怎么看

1. mock_*.jsonl 有 20000/21002/21003 收包：
   PC vmrp 已经跑到网络层，下一步继续改 mock 协议。

2. vmrp 窗口出现机甲风暴 loading/登录：
   单游戏启动成功。

3. 只打开 vmrp 空白/游戏列表：
   命令行启动方式不匹配，需要手动在 GUI 里选择 gwy/jjfb.mrp，或者看 vmrp 源码 main.c 的 argv 解析。

4. 日志里出现 sdk_key / mrc_loader / cfunction 缺失：
   文件系统位置不对，把 feedback 发回来继续调 fs root。

5. 无任何 mock 收包：
   要么没有进游戏，要么连接目标是硬编码 IP，PC 端需要进一步做 WinDivert/hosts/源码 patch。
