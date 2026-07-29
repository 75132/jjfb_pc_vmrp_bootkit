# P15 0x101AB Protocol Closure

Date: 2026-07-30  
Primary: `game_files/mythroad/240x320/gwy/jjfb.mrp`  
Resume: `JJFB_304BF0_RESUME_MODE=direct_lr`  
Low mem: `JJFB_MAP_LOW_GUEST_MEM=1`  
Path-A product default: synthetic code5 (`downVersion`) retained

## Verdict

**B54 Event15 的直接生产链已静态+运行时闭合到 `0x101AB` 字节流。当前阻塞不是“未知 B54 producer”，而是宿主仍只有 `SYNTHETIC_CODE5_COMPAT`，从未出现 `event_code=15` 的真实输入帧。**

成功条件 A–D（自然 code15→E6C、第六资源、新 UI、ANI）**未达成**。按允许的硬停止交付：缺失输入源 + 完整 ABI。

## 1. 冻结基线 / 构建身份

见 `reports/P15_BUILD_IDENTITY.txt`：

| Item | Value |
|------|-------|
| Parent commit | `293cabfff379fed04e6facc12b30644d71b4bcb4` |
| `main.exe` | `7c39d8f0…be2de` |
| `JJFB_Launcher.exe` | `b1187b00…a9497` |
| `jjfb.mrp` | `52c13182…5fc036` |
| Layer-1 SHA | `c789a129…bffda`（相对 P14 未回退） |

## 2. 静态闭合（robotol.ext `code_base=0x2D8DF4`）

```text
0x30D2F8  10165 trampoline  (sole BL→0x30D24C @ 0x30D2FA)
→ 0x30D24C(buffer=r0, capacity=r1)
→ 0x30D2AA sendAppEvent(0x101AB)  r1=buf r3=2
→ 0x30D2B0 r0 = initial parse cursor (NOT status, NOT bytes_written)
→ type / BE32 payload_len via 0x31103C
→ 0x2E4D6C → 0x2E4EA4 BE16 event_code → 0x2E4EEE B54 enqueue
→ 0x2DC8D4 → 0x2E2520 → (code15) 0x2E5E60 → R9+0xE6C
```

全量 in-place fill 必须返回 **cursor=0**（产品行为正确）。

## 3. 运行时采证（P15 TRACE）

`reports/P15_101AB_TRACE.csv` 完整记录同一 `frame_id`：

`30D24C → 30D2AA → PLAT_FILL → 30D2B0(cursor=0) → type/len → 2E4D6C → … → 2E4EA4 → node code=5 → B54 enqueue → 2DC8D4 → 2E2520`

| Frame | type | header | event_code | with_rec | dump |
|-------|------|--------|------------|----------|------|
| empty | 2 | 5 | **5** | 0 | `out/p15_101ab/frames/frame_2.bin` (19B) |
| downVersion | 2 | 5 | **5** | 1 | `out/p15_101ab/frames/frame_3.bin` (46B) |

**从未出现 event_code=15 / 2E5E60 / E6C_NATURAL_STORE。**

## 4. 传输分类（确定）

| 层级 | 分类 |
|------|------|
| 平台 ABI 形态 | receive-into-buffer（游标 R0） |
| **已闭合 producer** | **`LOCAL_BOOTSTRAP_STREAM`**（`SYNTHETIC_CODE5_COMPAT`） |
| Downstream | B54 queue（消费，非字节来源） |
| `NETWORK_RECEIVE_STREAM` | 未证明 |
| `CALLBACK_PAYLOAD_COPY` | 排除 |

目标（producer 闭合后）：`TRANSPORT_QUEUE`。

## 5. 本轮实现

- `platform_101ab_decode_frame()` + 单元测试 OK  
- `Platform101AbProvider`：synthetic / queue / capture-replay（code15 必须 SHA 溯源）  
- `product_101ab_trace`：18 站点统一 `frame_id`  
- 文字研究开关 `JJFB_TEXT_PARAM0_XY=1`（**未改产品默认**）  
- Runner：`research/runners/RUN_P15_101AB_CLOSURE.ps1`

## 6. 矩阵结果（`P15_FRAME_MATRIX.csv`）

| Group | Hold | Layer-1 | Resources (unique BMP) | text app7 | 101AB codes | code15 | E6C |
|-------|------|---------|------------------------|-----------|-------------|--------|-----|
| baseline ×2 | 180s | PASS | **5**（矩阵列曾误计 6） | YES | 5 only | NO | NO |
| textA/B 70s | 70s | 门禁噪声 | 未完整到 app7 | 多数 NO | 5 | NO | NO |
| provider_synth | 90s | PASS | 仍 ≤5 | YES | 5 | NO | NO |

五资源仍为：`loadingbar | bar | textbar | topleft | topright`。稳定在 `NO_NEW_RESOURCE_FOR_MS / stable_after_res5`。

## 7. 文字可见性 A/B

param0 LE raw：`72 00 DB 00…` → halfwords **114, 219**。

- **A（产品默认 `{y,x}`）**：y=114,x=219 → 绘制 (219,114)；`handled=1` hex=`8F7D51654E2D`（载入中）。  
- **B（`JJFB_TEXT_PARAM0_XY=1`）**：70s 轮次未采到 app=7 绘制（仅 app=10 immediate）；**不足以翻转产品合同**。  
- x=219 在宽 240 屏上三字极易裁切；首帧 SHA 不变说明文字不在 Layer-1 门禁帧内。

## 8. 硬停止：缺失的原始输入源

**缺失：** 原版喂给 `0x101AB` 的后续协议帧序列中，至少一条 `event_code=15` 且 body 满足 `0x2E5E60` 的完整帧。

**不是：** 未知 B54 producer（已闭合）。

**唯一下一步：** 从原版启动器/真机对同一次 `0x30D24C→0x101AB` 做全量 buffer 抓包（SHA + 序号），经 `CAPTURE_REPLAY_RESEARCH` / `TRANSPORT_QUEUE` 重放，让 guest 自然 `2E4EEE→2E5E60→E6C`。禁止改 code5→15 / 宿主 enqueue 15 / 强写 E6C。

## 交付物

```text
reports/P15_101AB_PROTOCOL_CLOSURE.md
reports/P15_101AB_TRACE.csv
reports/P15_FRAME_MATRIX.csv
reports/P15_RUNTIME_CONTRACT.json
reports/P15_BUILD_IDENTITY.txt
out/p15_101ab/
```
