# JJFB-only v7：只测试 PC 模拟器直接启动机甲风暴 jjfb.mrp

## 这次路线纠正

本包不再走：

```text
gwy 社区大厅
gamelist 更新门槛
portproxy / IP 劫持
```

本包只做一件事：

```text
让 vmrp 一启动就直接加载 机甲风暴 jjfb.mrp
```

方式是：

```text
把 gwy/jjfb.mrp 复制成 vmrp 固定启动入口 mythroad/dsm_gm.mrp
```

因为前面日志已经证明 vmrp 固定启动的是：

```text
mythroad/dsm_gm.mrp
```

所以如果想直接启动 jjfb，最直接就是替换这个入口。

## 使用方法

解压到原来的：

```text
C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit
```

覆盖同名文件。

然后运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_BOOT_JJFB_ONLY_V7.ps1
```

不需要管理员。

## 它会做什么

```text
1. 找 vmrp 的 main.exe
2. 找你的 jjfb.mrp：
   - game_files/mythroad/240x320/gwy/jjfb.mrp
   - game_files/mythroad/gwy/jjfb.mrp
   - runtime/.../mythroad/gwy/jjfb.mrp
3. 把完整 mythroad/240x320 文件树复制进 vmrp mythroad 根目录
4. 写 sdk_key.dat 到多个可能位置
5. 把 jjfb.mrp 复制成 mythroad/dsm_gm.mrp
6. 同时保留：
   mythroad/jjfb.mrp
   mythroad/gwy/jjfb.mrp
7. 启动 vmrp
8. 等 60 秒
9. 收集日志 zip
```

## 成功标志

你要看的不是冒泡社区，而是：

```text
机甲风暴标题 / loading
正在登录
正在获取角色资料
连接服务器
连接超时
```

如果仍然出现冒泡社区，说明 dsm_gm 没替换成功。

如果黑屏/闪退/报错，发：

```text
logs/jjfb_only_feedback_*.zip
```

## 失败后怎么判断

- `cann't find sdk key`：sdk_key 路径还不对。
- `mrc_loader.ext / robotol.ext`：jjfb 需要额外文件或工作目录。
- 直接返回/黑屏：jjfb 需要 gwyblink/_mr_param，上层参数没注入。
- 出现连接 20000：直接启动成功，后续再处理游戏网络。
