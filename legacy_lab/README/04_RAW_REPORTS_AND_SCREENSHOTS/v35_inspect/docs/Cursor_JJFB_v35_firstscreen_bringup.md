# Cursor JJFB v35 — First-screen bring-up

## 阶段目标

```text
看到机甲风暴原版启动/加载相关画面
允许：边框缺失、部分图片缺失、文字不完美、网络没通
不允许：host overlay、自定义启动进度条、假冒游戏画面
```

优先级：

```text
最小可见首屏 > 完整 UI 边框 > 原生 refresh > 网络登录
```

## 本轮改动

1. **保留 chrome guard**：`JJFB_CHROME_SKIP_310BB4=1`，不让 `wy_jiao/wy_xian/jiantou` 阻塞
2. **DrawRect → guest RGB565 + DEBUG_PRESENT**（游戏自己画的矩形能上屏）
3. **0x11F00**：暗底清屏一次；ASCII 字模；GBK 占位；扫描 code 对象指针里的字符串
4. **`[JJFB_FIRST_SCREEN]`** 日志：state 变化、资源 open、2d92dc 名字、11F00 ASCII
5. **0x10134 fail-open**：不阻塞首屏
6. **资源优先**：`slogo` / `loadingbar` / `logo` / `login` / `loading` / `title` / `start`

## 明确不是

```text
DEBUG_PRESENT ≠ 原生 DispUpEx / mrc_refreshScreen
不做 host overlay（禁止窗口写 “JJFB BOOTING”）
不追完整 chrome / 网络
```

## 已知证据（robotol 内嵌名）

```text
slogo!157!58.bmp
loadingbar!201!29.bmp
机甲 / 风暴 / 登录 / 加载 （GBK 字符串在 robotol_ext）
```

## 跑法

```powershell
.\RUN_V35_FIRSTSCREEN.ps1
```

## 成功标准

1. 窗口不再纯白
2. 画面来自 guest buffer（11F00 / DrawRect），不是 host overlay
3. 能看到启动/加载/登录/标题文字中的任意一种（或至少其 ASCII/占位）
4. 日志区分 DEBUG_PRESENT vs native refresh
