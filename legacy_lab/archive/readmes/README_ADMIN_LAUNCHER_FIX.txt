# v5 管理员启动补丁

你刚才的错误不是脚本坏了，而是当前 PowerShell 不是管理员权限。

把这两个 bat 放到 jjfb_pc_vmrp_bootkit 根目录，然后双击：

RUN_AS_ADMIN_LOCAL_NET_V5.bat

弹出 UAC 时点“是”。

测试完恢复网络别名时双击：

RUN_AS_ADMIN_RESTORE_LOCAL_NET_V5.bat

中文乱码只是 PowerShell 编码问题，不影响判断。
