# LATEST_DIRECTION_v49 — 当前最高优先级方向

当前方向以 v49 为准：

```text
不是还原机甲风暴 UI；
不是推 jjfb 内部状态；
不是重做动画；
而是仿冒泡网游/gwy 的启动游戏方式，绕过 gwy 外壳强制更新，补齐 startGame/runapp 对 jjfb 的启动契约。
```

主线任务：

```text
建立 GWY Launcher Shim：
1. 不启动/不依赖 gwy UI 强制更新流程；
2. 读取/模拟 cfg index=36；
3. 使用参数：napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink；
4. 设置 cwd/root/path/_gwyblink/nextid/ncode/narg1 等上下文；
5. 调用 startGame/runapp 等价入口；
6. 让 mrc_loader.ext -> robotol.ext -> 原始 jjfb.mrp 自然运行。
```

应停止作为主线的内容：

```text
FORCE ui_mode=0x45
手动写 AC8
手动写 progress_count
progress driver
为了 splash 动画扫 eventcode
host overlay / fake UI
```

保留为平台能力的内容：

```text
_mr_c_load / _strCom 601/800/801
mrc_loader.ext / robotol.ext 加载
mrc_init ret=0
timer/event 基础分发
240×320 与 0xF81F colorkey
obj=0 skip
真实资源 blit
```
