# JJFB Next-Screen Final (P11–P14)

Date: 2026-07-30  
Primary target: `game_files/mythroad/240x320/gwy/jjfb.mrp`  
Product resume: `direct_lr` (unchanged)

## 1. 本轮是否出现第 6 个资源

**否。** 连续两轮 180s 仍稳定 **5** 个自然资源：

`loadingbar!201!29.bmp | bar!16!18.bmp | textbar!120!30.bmp | topleft!15!5.bmp | topright!12!4.bmp`

## 2. 是否出现新 UI/文字/动画

**是 — 真实 guest 文字绘制（准则 B）。**

- `0x11F00` **app=7**（caller `0x2F2360`）解析到 guest 文本对象并经 UC2 栅格化：
  - `handled=1 app=0x7 str_va=0x2A83F4 @ (219,114) hex=8F7D51654E2D`
- `app=0x10 code=0x3E8` 已识别为 **非文本 immediate**，不再误用 `last_str_va`（避免首帧污染）
- **无** 新 ANI / 无第 6 BMP / 无下一完整界面切换
- Layer-1 首帧 SHA 与冻结基线一致：`c789a129…bffda`（文字发生在首帧门禁捕获之后的 chrome 路径上）

## 3. 最远自然执行位置

```text
family case5 @0x30E188 → 0x2D9601（低内存映射后不再 P3 fault）
→ Path-A 0x2E2520 event_code=5 index=2 → 0x2E4040
→ 0x2F68E4 → 0x2DADC4 → 0x2FC26C (UI_MODE→0x3)
→ downVersion / downVersion.v 自然 VFS open
→ 稳定空转 PC≈0x304558（sendAppEvent）
E6C=0；未进 0x2E4020 / 0x2E5E60；未进 0x2DC4D8
```

## 4. 真正根因（本轮闭合部分）

| 曾误判/旧阻塞 | 本轮结论 |
|---------------|----------|
| `0x304BF0` resume | 已排除（P10）；保持 `direct_lr` |
| case9 / epilogue R0=handle | 禁止；非第六资源主因 |
| late P3 @ `0x2D960E` addr `0x1E205` | **family case5** 把 **event code `0x1E201`** 当对象指针；低内存 RW map 消除 UC_FAULT |
| BCS @ `0x2E2532` word0=0 | **已过时**：`PATH_A_EVENT_CONTRACT` 下 word0=5，进入 `0x2E4040` |
| 第六资源仍缺 | **B54 无自然 `event_code=15`** → 无法 `0x2E5E60` 写 E6C；Path-A `101AB` 只编 **code=5 (downVersion)** |

## 5. 修改了哪个通用平台合同

1. **Guest 低地址映射**（`vmrp.c`）：`[0, CODE_ADDRESS)` 零填充 RW（可用 `JJFB_MAP_LOW_GUEST_MEM=0` 关闭）  
2. **Family 投递 ABI**：APPLY_ABI 下 **不再** 把 `guest_context` 塞进 `r2`  
3. **`0x11F00` 文本 ABI**（`platform_text_api.c`）：按 app=7 合同解析文本对象；拒绝 app=10 immediate；禁止错误缓存串画

未改：JJFB 固定 ERW 补丁、强写 B71/E6C/index、epilogue 产品化。

## 6. JJFB 180/300 秒稳定性

| 项 | P14 r1 | P14 r2 |
|----|--------|--------|
| Hold | 180s | 180s |
| Layer-1 | PASS | PASS |
| 首帧 SHA | `c789a129…bffda` | 同左 |
| resources | 5 | 5 |
| text app7 | YES | YES |
| alive@end | YES | YES |
| P3_FAULT / READ_UNMAPPED | NO | NO |
| 300s | 未另跑；180s×2 无回退 |

## 7. 其他网游回归

本轮未跑其他网游全量回归（任务允许 JJFB 出成果后再做）。低内存 map 与 `0x11F00` 解析均为通用平台合同，默认对非 JJFB 文本路径保持“解析失败则不画”。

## 8. 仍然存在的唯一阻塞

**唯一阻塞：** 缺少 **B54 上自然 `event_code=15`**（及 E6C 形态 payload）→ 无法 `0x2E2520 → 0x2E4020 → 0x2E5E60` 完成 `E6C_NATURAL_STORE`，post-drain 无法推进到第六资源/下一界面。

**下一步只需一条动作：**  
对 B54 / Path-A 生产者做定向采证，找到 **应发出 code=15** 的 guest/协议路径；仅当 producer↔consumer 证据证明 `101AB` 流应含 code15 时，才扩展 `platform_101ab_fill_path_a`。禁止宿主 enqueue Event15 / 强写 E6C / B50→2E2520。

---

Artifacts: `reports/JJFB_NEXT_SCREEN_MATRIX.csv`, `reports/JJFB_RUNTIME_CONTRACT.json`, `reports/JJFB_FRAME_PROGRESSION.csv`  
Archives: `out/p12_lowmem_ab/`, `out/p14_text_ab/`
