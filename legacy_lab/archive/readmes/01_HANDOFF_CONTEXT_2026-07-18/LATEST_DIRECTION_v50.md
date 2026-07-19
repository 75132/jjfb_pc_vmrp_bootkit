# LATEST_DIRECTION_v50 — GWY Launcher Shim + canonical resource root

生成：2026-07-17

## 当前唯一主线

```text
目标不是还原或伪造《机甲风暴》的 UI。
目标是模拟冒泡网游/GWY 已完成外壳更新检查后的 startGame/runapp，
把 cfg index=36、完整资源根和平台启动上下文交给原始 gwy/jjfb.mrp。
```

## v50 已实现的静态改动

1. `JJFB_GWY_LAUNCHER_MODE=1` 时入口由旧的 `dsm_gm.mrp` 改为 `gwy/jjfb.mrp`。
2. cfg index=36 参数保持：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

3. 文件系统新增 canonical 映射：

```text
mythroad_root = runtime/.../mythroad/240x320
gwy_root      = runtime/.../mythroad/240x320/gwy
```

支持：

```text
gwy/*
mythroad/gwy/*
mythroad/240x320/*
240x320/*
无前缀的 jjfbol/*、gifs/*、save/*、sound/*
```

4. canonical 路径优先于旧的 runtime 扁平副本，防止 `dsm_gm.mrp` 或旧 `mythroad/gwy` 掩盖真实路径错误。
5. 所有 guest `open` 输出 guest/host/PC/LR 和 miss 日志。
6. host code=8 appInfo 不再错误写死 `482/1`；改为读取原始 `jjfb.mrp` 头部：

```text
APPID   = 400101
APPVER  = 12
```

`nextid=482` 是 GWY cfg 启动字段，不是 MRP APPID。

## 资源静态事实

```text
game_files/mythroad/240x320/gwy/jjfb.mrp     存在
jjfbol/                                       119 个文件
gifs/                                         88 个文件
save/                                         空目录但必须保留
sound/                                        4 个文件
```

`mrc_loader.ext` 与 `robotol.ext` 位于 `jjfb.mrp` 内部，不是主机顶层文件。

## 本轮 runner

```powershell
.\RUN_V50_GWY_LAUNCHER_MODE.ps1 -Seconds 25
```

runner 会：

1. 完整复制 `game_files/mythroad/240x320` 到 runtime，保持目录结构；
2. 清除 v49 旧的 UI/state force 环境变量；
3. 重新编译所有 32 位对象；
4. 启动 canonical `gwy/jjfb.mrp`；
5. 保存 `logs/v50_gwy_launcher_mode_stdout.txt`；
6. 生成 `reports/v50_gwy_launcher_run_result.md`。

## 动态判定顺序

```text
A. 先确认 [JJFB_GWY_ROOT] 指向 runtime canonical tree；
B. 再确认 [JJFB_FILEOPEN] guest="gwy/jjfb.mrp" ok=1；
C. 再处理第一个真实 FILEOPEN_MISS；
D. 无路径 miss 后，检查 _strCom 601/800/801 与 mrc_init；
E. loader 通过后，才看原始游戏自身网络/检查更新/登录。
```

不要由是否出现 splash/loadingbar 判断启动成功。

## 尚未在当前 Linux 环境完成的证据

Windows/MSYS2 32 位二进制实际运行尚未执行，因此目前不能声称游戏已经进入网络或登录。下一条有效证据必须来自 v50 runner 的真实日志。
