# JJFB PC vmrp Bootkit — PC端机甲风暴启动测试包

## 目标

这个包改为 PC 端路线：使用 `vmrp` Windows 版作为 MRP 运行器，准备 `mythroad` 文件树，启动本地 mock server，尝试直接启动：

```text
gwy/jjfb.mrp
```

并自动保留：

```text
logs/mock_*.jsonl
logs/vmrp_stdout_*.txt
logs/vmrp_launch_attempts.json
logs/netstat_*.txt
logs/fs_snapshot_*.csv
logs/feedback_*.zip
```

## 你只需要放两个东西

### 1. 游戏文件

把你已有的 `mythroad` 整个目录内容放到：

```text
game_files/mythroad/
```

推荐结构：

```text
game_files/mythroad/
  gwy.mrp
  sdk_key.dat
  gwy/
    cfg.bin
    jjfb.mrp
    jjfbol/
    gbrwcore.mrp
    gbrwshell.mrp
    font.mrp
    resmng.mrp
    vdload.mrp
    dload.mrp
    reglogin.mrp
```

不要只放 `jjfb.mrp`，否则很容易缺公共模块/资源/启动上下文。

### 2. Python

Windows 上需要能执行：

```powershell
python --version
```

## 一键启动

在本包根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_FULL_TEST.ps1
```

脚本会自动：

```text
1. 下载 vmrp Windows release
2. 解压 vmrp
3. 寻找 vmrp/main.exe
4. 把 game_files/mythroad 部署到 vmrp 的 mythroad 文件系统目录
5. 写 sdk_key.dat
6. 启动本地 mock server：21002 / 21003 / 20000 / 6009
7. 尝试 hosts 域名映射到 127.0.0.1
8. 尝试多种命令行方式启动 gwy/jjfb.mrp
9. 收集日志和网络连接状态
10. 打包 feedback zip
```

## 注意

我没有把第三方 `vmrp.exe/main.exe` 直接塞进包里，而是脚本从官方 release URL 下载。这样包更干净，也避免误塞不可控二进制。你也可以手动把 vmrp Windows 版解压到：

```text
runtime/vmrp_win32/
```

再运行 `RUN_PC_FULL_TEST.ps1`。

## 成功标志

成功不要求进入角色界面。出现任意一个就算路线打通：

```text
vmrp 窗口打开机甲风暴画面
出现 loading / 正在登录 / 连接超时
mock_server 收到 20000 或 21002/21003 新包
logs 里出现 jjfb / robotol / mainmenu / mrc_loader
```

## 如果失败

把最新的：

```text
logs/feedback_*.zip
```

发回来。
