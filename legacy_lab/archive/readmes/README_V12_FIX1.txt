# V12 parse_pe fix1

你刚才遇到的是脚本 bug，不是主路线失败。

错误原因：
PE section header 的 struct.unpack_from("<IIIIIIHHI") 只返回 9 个字段，
旧脚本却写成 10 个变量接收。

修复方式：
把本包解压到 jjfb_pc_vmrp_bootkit 根目录，然后运行：

powershell -ExecutionPolicy Bypass -File .\FIX_V12_PARSE_PE_BUG.ps1

再重新运行：

powershell -ExecutionPolicy Bypass -File .\RUN_PC_BINARY_PATCH_AND_BOOT_V12.ps1
