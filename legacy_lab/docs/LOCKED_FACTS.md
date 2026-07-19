# 锁定事实速查

## 目标

在 `ui_mode=0x45` 下跑通启动检查 UI；不是完整模拟器，不先追登录/网络。

## 禁止

- host overlay / 假 UI
- 把 `DEBUG_PRESENT` 说成原生 refresh
- `0x10134` fail-open 或返回 object
- UCRT64 编译
- progress driver 当最终方案
- 把 `0x2EFC58/6C` 当 STR writer
- 跳过 0x45 追网络

## 结构

```text
ERW (常 0x2B1850)
  +0x8D0  ui_mode
  +0xAC8  splash 计数/门
  +0xB6C  深路径门（v49）
  +0x134D 成功段门（v49）
  +0x1350 与 100 比较
  +0xBA0  splash 进度块基址
       +0x20 bar_obj
       +0x24 textbar_obj
       +0x28 loadingbar_obj
       +0x2C progress_count
```

## 调用

```text
0x10140 → 0x306305 → 0x306344 → (ui_mode==0x45) → 0x30662C → 0x2EF86C
自然参数：r0=0x45 r1=0x13 r2=0xFFFFFFFF r3=0x2B199C
```

## 屏

240×320；colorkey `0xF81F`；loadingbar 布局约 `(19,220)`（轴修复后）。
