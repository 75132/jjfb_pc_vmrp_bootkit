# v43：原资源审计 + splash 状态机（服务“跑机甲风暴”）

生成时间：2026-07-17

## 0. 一句话

```text
原版资源能请求/构造；透明色与 240×320 轴已修对；
但 AC8 无自然写入、progress count=0 → slogo/loading 仍无法同轮自然衔接，bar 不画。
还不是完整原版启动流程。
```

---

## 1. 原资源审计

见 `resource_audit_v43.md` 与 contact sheet。

| 资源 | 尺寸 | 0xF81F | 说明 |
|------|------|--------|------|
| loadingbar | 201×29 | **202 px（四角全是）** | 必须 colorkey |
| bar | 16×18 | **136 px** | 必须 colorkey |
| slogo | 157×58 | 0（cache 无品红） | 默认仍试 key，本轮 skipped=0 |
| textbar | 120×30 | 0 | 四角 `0x4162`，不强制跳 |

对象字段旁证：`loadingbar obj+0x1C = 0xF81FF81F` → host blit `source=obj+1C`。

证据：

```text
[JJFB_COLORKEY] loadingbar key=0xF81F source=obj+1C
[JJFB_BLIT_KEYED] loadingbar x=19 y=220 drawn=5627 skipped=202
```

---

## 2. splash 函数图

```text
0x2EF86C  SPLASH_ENTER
   │
   ├─ read ERW+0xAC8 @ 0x2EF8A0
   │     AC8>0 ──► 0x2EF9A7 2d92dc(slogo) ──► 0x2EF9DF 2EC6B0(slogo)
   │     AC8<=0 ─► skip slogo block
   │
   └─ 0x2EF9F4..  loading path
         0x2EFA33 2d92dc(loadingbar) → slot@+0x28
         0x2EFA43 2d92dc(bar)        → slot@+0x20
         0x2EFA53 2d92dc(textbar)    → slot@+0x24
         0x2EFA97 2EC6B0(loadingbar)
         0x2EFA9E..0x2EFADE progress loop (最多 12 格)
              *(ERW+0xBA0+0x2C) = progress count
              *(ERW+0xBA0+0x20) = bar object
              count > idx 才 bl 0x2EC6B0(bar)
```

`2EC6B0` → `blx *(ERW+0x150C)`；当前 FP=`0x270F` → host blit + guard（`r0=1`）。

---

## 3. 布局轴（已完整）

```text
2F9968 → 240   // x = (W - bmp.w)/2
2F995C → 320   // y = H - 100
```

```text
[JJFB_LAYOUT] loadingbar x=19 y=220
```

（v41 只有 x=19 y=140；现 y 已到 220。）

---

## 4. AC8 状态机

| 模式 | 现象 |
|------|------|
| `natural` | AC8 一直为 0；只走 loadingbar；**无 AC8_WRITE** |
| `force_loading` | 同 natural（保持 0） |
| `force_slogo_once` | 先画 slogo，再释放 AC8=0；随后进 chrome `wy_*`，**未再请求 loadingbar** |

结论：

```text
本环境未见 guest 自然写 AC8。
AC8 像“启动前半是否已准备/是否显示 logo”的外部标志，缺上游写入。
长期固定 AC8=1 会卡 slogo；释放后也不自动拼上 loading 同轮流程。
```

默认：`JJFB_SPLASH_AC8_MODE=natural`（不再默认 SLOGO_NUDGE）。

---

## 5. bar/textbar 为何不显示

```text
[JJFB_PROGRESS] idx=0..11 count=0 bar_obj=0x6BFE40 name=bar!16!18.bmp SKIP(count<=idx)
```

- bar 对象已挂到 `BA0+0x20`
- **progress count (`BA0+0x2C`) 恒为 0** → 循环全部 SKIP
- 不是 2EC6B0 guard 单独导致；根因是进度未推进

下一棒应追：**谁写 `ERW+0xBA0+0x2C`**（timer / 加载回调 / 网络进度）。

---

## 6. 当前阻塞（游戏推进视角）

```text
1. AC8 无自然写入 → slogo/loading 不能原版同轮衔接
2. progress count=0 → bar 不画，启动进度停住
3. ui_mode 卡 0x45；JJFB_NET 仍为 0
4. fp150C=0x270F → 仍依赖 host blit（DEBUG_PRESENT）
```

---

## 7. 跑法

```powershell
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Ac8Mode natural
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Ac8Mode force_slogo_once
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Ac8Mode force_loading
python tools\audit_v43_resources.py
```

---

## 8. 下一步（仍服务往后跑）

```text
P0：谁写 BA0+0x2C（progress）/ 谁写 AC8
P0：progress>0 后 bar 是否画出；ui_mode 是否离开 0x45
P0：login/server/initNetwork
P3：chrome 继续 guard
```
