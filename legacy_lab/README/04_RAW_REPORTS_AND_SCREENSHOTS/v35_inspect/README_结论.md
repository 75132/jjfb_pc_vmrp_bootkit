# JJFB v35b 结论包 — 为什么你看到的不是原版首屏

日期：2026-07-17

## 直接回答

你说得对：**之前窗口里看到的不是机甲风暴原版启动画面。**

那是：

- guest RGB565 暗底清屏
- `0x11F00` 反复画的 token ASCII：`CYFmhdNmS1roRgroRa==`
- DEBUG_PRESENT 把上述 buffer 显示出来

**不是** `slogo` / `loadingbar` / 原版 logo。

## 根因（已修）

handler@0x306305 的 switch 读的是 `ER_RW+0x8D0`：

```text
值 0x45 → bl 0x2EF86C  ← 真 splash（slogo/loadingbar）
值 0x01 → 另一条 UI 分支（旧 FORCE 误用了这条）
```

旧逻辑 `FORCE state 0→1` **正好跳过了原版 splash**。

已改为：

```text
FORCE state/ui_mode → 0x45
```

## 修复后证据（logs/v35_splash45_stdout.txt）

```text
FORCE state/ui_mode 0x0 -> 0x45
SPLASH_ENTER #1 lr=0x306631 ui_mode=0x45
2d92dc name="loadingbar!201!29.bmp"
2d92dc name="bar!16!18.bmp"
2d92dc name="textbar!120!30.bmp"
```

真 splash 函数已进入，并开始请求加载条资源。

## 仍未完成（所以画面还可能不像原版）

1. `slogo!157!58.bmp` 本轮仍未出现（splash 前半分支可能失败）
2. `drawBitmap = 0`：资源名请求了，但没有真正 blit 到屏幕
3. `0x10134` 当前是 fail-open；对 loadingbar 它带着 `app=0x2D8A`（像 bitmap 尺寸）被调用——空成功可能导致 bmp 对象为空，所以看不见图

## 下一步（专攻可见原版内容）

```text
0x10134 真构造/解码 bitmap（loadingbar/slogo）
→ 确认 blit 路径（drawBitmap / 其它）
→ DEBUG_PRESENT 显示 guest 已画内容
仍不做 host overlay
```
