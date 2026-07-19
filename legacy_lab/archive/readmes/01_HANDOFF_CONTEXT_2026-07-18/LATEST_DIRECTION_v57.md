# 最新路线：禁止 host UI（v57 纠正后）

## 硬规则（再强调）

```text
❌ host 贴 splash / 写死坐标 / 假动画
❌ FORCE ui_mode / AC8 / progress 当正式方案
✅ 只补 GWY 启动契约 + 平台 API，让 jjfb 自己画
```

## v57 有效残留

- `0x10134` size-map：给 guest 真像素（不负责上屏布局）
- DrawRect `axis_remap`：320 布局 → 240 屏
- guest `2FC418` → `ui_mode=0x45`（仍属探针，非正式）

## v57 作废

- `GWY_SPLASH` host blit（已默认禁用）
- host 暗底 present（已删除）

## v58

让 **guest `2EC6B0`** 用游戏坐标自然 blit；查为何未进入该路径。
