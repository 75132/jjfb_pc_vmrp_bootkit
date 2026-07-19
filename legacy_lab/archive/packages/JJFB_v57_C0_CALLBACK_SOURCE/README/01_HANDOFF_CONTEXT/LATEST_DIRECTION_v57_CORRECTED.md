# 纠正：绝对禁止 host 自绘 UI

## 用户裁定（已接受）

```text
绝对不能 host 自己绘制 / 自己摆 splash。
看到的 loadingbar 位置与动画若不是游戏逻辑跑出来的，一律作废。
```

## v57 错误（已认）

`jjfb_gwy_present_splash_assets` 用 host 写死坐标贴原版素材 = **假 UI**，违反项目硬规则。

## 已立刻改正

1. `JJFB_GWY_SPLASH_BLIT` **默认关闭**；仅 `=1` 才进废弃探针并打 WARNING。
2. 去掉 bring-up 暗底 `guiDrawBitmap` host present。
3. `RUN_V57_*.ps1` 显式 `JJFB_GWY_SPLASH_BLIT=0`。

## 仍允许（平台，不是画 UI）

```text
0x10134 按尺寸构造像素缓冲（给 guest 用）
DispUpEx / DrawRect 把 guest 已画内容刷到 SDL
axis_remap 把屏外几何拉回 240 画布
```

## 正确下一步（v58）

```text
追 guest 自然 2EC6B0（用游戏自己的 x,y）去 blit；
追 AC8/progress 的自然生产者（禁止 host 写）；
禁止再开 GWY_SPLASH / EAGER_BLIT 当正式方案。
```
