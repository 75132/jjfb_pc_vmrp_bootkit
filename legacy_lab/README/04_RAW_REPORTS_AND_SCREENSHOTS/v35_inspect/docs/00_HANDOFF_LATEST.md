# JJFB / PC-vmrp 交接包（最新 · v35 first-screen）

生成时间：2026-07-17（v35 首屏 bring-up）  
用途：发给 GPT / 其他模型继续推进，**勿从零猜测、勿回退已稳定层**。

---

## 0. 一句话状态

```text
阶段目标已收敛：先看到机甲风暴原版启动/加载首屏（允许边框/图片缺失）。
不做 host overlay；DEBUG_PRESENT 只显示 guest buffer。
【关键修复】旧 FORCE state→1 会跳过真 splash；应 FORCE → 0x45 才会进 0x2EF86C。
已验证 SPLASH_ENTER + loadingbar!201!29.bmp 请求出现。
下一步：0x10134 真构造 bitmap（当前 fail-open 导致无图可 blit）。
原生 DispUpEx / mrc_refreshScreen 仍为 0。
```

**不要：** host overlay、假冒游戏 UI、整表 retarget、假 refresh、优先深挖 wy_jiao 边框。

详见：`Cursor_JJFB_v35_firstscreen_bringup.md`

---

## 1. 目标

让 PC 版 **vmrp** 跑通：

```text
start.mr → mrc_loader.ext → robotol.ext → modules → network (20000 / 21002 / 6009)
```

**当前阶段（v35）：** 最小可见首屏 > 完整 chrome > 原生 refresh > 网络。

当前卡在 **UI present / 绘制语义**，不是 loader、不是 sdk_key、不是 timer 基础。

---

## 2. 路径与环境（已验证，勿改）

| 项 | 路径 / 结论 |
|----|-------------|
| 项目根 | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit` |
| 源码 | `runtime\vmrp_src_build_v27\vmrp-master`（主改 `bridge.c`） |
| 运行时 | `runtime\vmrp_win32\vmrp_win32_20220102` |
| 最佳 MRP | `logs\jjfb_801diag_only6_skip0_v27.mrp` |
| robotol dump | `logs\robotol_ext.bin`（VA base **`0x2D8DEC`**） |
| 编译 | **MSYS2 mingw32 i686**，`C:\msys64\mingw32\bin`，**禁止 UCRT64** |
| 依赖 | vendored unicorn-1.0.2-win32 + SDL2-2.0.10 |
| 日志 | runtime CWD 的 `jjfb_loader_stdout.txt`（`freopen`；管道重定向会空） |
| 低内存 | `vmrp.c` map `0..CODE_ADDRESS` |

### 续写文档（按时间）

| 文件 | 内容 |
|------|------|
| `AAA-tip.md` / `AAA-tip2.md` | 早期约束 |
| `Cursor_JJFB_v30_*.md` | timer / refresh / network 方向 |
| `Cursor_JJFB_v31_12340_text_present_chain.md` | 12340 / 屏参 / 打到 present 中层 |
| `Cursor_JJFB_v32_11F00_blit_2f4494_resources.md` | present 中层 + 2f4494 定性 |
| `Cursor_JJFB_v33_11F00_glyph_chrome_native_present.md` | **当前主文档** |
| 本文件 | 总交接包 |

### 证据日志（v33）

| 文件 | 说明 |
|------|------|
| `logs\v33_shadow_heal_stdout.txt` | **最新**：shadow gfx heal + ASCII glyph + code 对象完好 |
| `logs\v33_glyph_ascii_present_stdout.txt` | 多字素 ASCII 首通 |
| `logs\v33_ctx_retarget_stdout.txt` | 反例：整表 retarget→c2u UNIMPL→code=-1 |

### 证据日志（v32）

| 文件 | 说明 |
|------|------|
| `logs\v32_nop4494_present_stdout.txt` | nop 2f4494 后 11F00+DEBUG present |
| `logs\v32_fix_drawrect_stdout.txt` | 303d94 FP→DrawRect，仍卡 310bb4 |
| `logs\v32_303d94_fp_stdout.txt` | 发现 r7=0x80278 |

---

## 3. 已稳定层（禁止回退）

这些已经通，**不要再当主问题**：

1. mingw32 + unicorn + 低地址 map  
2. loader：`mrc_loader.ext` → 801 → `robotol.ext`（字符串 patch `cfunction.ext`→`robotol.ext`）  
3. host `mrc_init(0) ret=0`（合成 `mrc_extChunk` + `sendAppEvent` hook）  
4. timer / `0x10140` / heap gates  
5. `mr_registerAPP` 恒成功  
6. refresh gate C44 via `0x2FC8B8`  
7. FORCE state 0→1（bring-up）  
8. 7D8 screen struct seed（见下）

### 屏参（不是 clock！）

```text
0x830 = 屏高（getter 2f9968）
0x834 = 屏宽（getter 2f995c）
禁止写 uptime 到这里

seed:
  824w = 240
  828y = 0
  830h = 320
  834w = 240
```

### `0x12340`（v31 已结案）

```text
逐字侧效应 / 文本度量
305e70 → 304550 → 0x12340
返回值被忽略；写 *param1
不是 present 阻塞点
```

---

## 4. v32 本轮做了什么

### 4.1 `0x11F00` — 已实现最小 drawText 副作用

调用链（每 tick）：

```text
2e87ac → (2f284c/2f4494) → 2e885a → 305bf4 → 305c34 → 2f2358 → sendAppEvent(0x11F00)
```

ABI（已确认）：

```text
app  = 7
code = 富文本/glyph 对象（非纯 C 字符串；dump 含 CYFmhNdN…）
p0   = pack:
       +0  y (i16)
       +2  x (i16)
       +0x2C  RGB888（观察为 FF FF FF）
```

Host 行为（`bridge.c`）：

- 在 `jjfb_screen565[240*320]` 写 **8×16 占位块**（用 p0 RGB）
- 日志 `[JJFB_DRAW_TEXT]` / `[JJFB_DEBUG_PRESENT]`
- dirty rect 调 `guiDrawBitmap` **仅作 DEBUG present**
- **禁止**把它标成原生 `mrc_refreshScreen`

最新证据（`v32_nop4494_present_stdout.txt`）：

```text
JJFB_DRAW_TEXT     > 0
JJFB_DEBUG_PRESENT > 0
enter 0x305bf4     > 0
DispUpEx           = 0
mrc_refreshScreen  = 0
```

窗口应能看到 (7,8) 附近色块。

---

### 4.2 `drawFP@1510` — 错误 seed 已禁用

```text
自然值常为 0x270F（无效）
曾错误 seed → drawBitmap bmp=0 240x0（假象）
现：不 seed，只 [JJFB_1510_WRITE] 观察
```

---

### 4.3 `2f4494` — 已完整定性（本轮核心 RE）

#### 它是什么

```text
UI 装饰框 / chrome 绘制（不是 wait/lock）
懒加载 bmp：wy_jiao* / wy_xian* / jiantou*（名在 robotol 字符串池）
资源名编码：name!w!h.bmp
mode 来自 2f284c → 2ea180，当前为 mode=0xa
```

#### 调用链

```text
2e87ac
  → bl 2f284c
    → bl 2ea180 (mode=0xa @ stack)
      → bl 2f4494
        → (init) 多次 bl 2d92dc 加载 bmp 对象 → 存 ERW+0xA64 等
        → 303d94
            → blx r7     ← 致命点
        → 循环 bl 310bb4 贴图
        → epilogue @ 2F4A70
  → 2e885a
  → bl 305bf4   ← drawText 路径（要到这里才有 11F00）
```

#### 根因（为什么“不返回”）

```text
303d94: tbl = *(ext_base=0x2D8DEC) = shadow@0x281EFC
         r7 = *(tbl + 0x1E8)

旧：*(shadow+0x1E8) = 0x80278（CODE 区垃圾）→ emu 停 stopAddr
现：jjfb_heal_shadow_gfx 已把 gfx 槽写成 host stubs
    *(shadow+0x1E8) = 0x2801EC（真 DrawRect）
    2f4494 仍 nop（放行后还会卡 310bb4 / 坏 h）
```

**禁止** 把 `*(ext_base)` 整表改成 host `mr_table`（会 UNIMPL c2u → text code=-1）。

补充实验：

| 实验 | 结果 |
|------|------|
| nop @ 0x2F4494 | 305bf4 / 11F00 / DEBUG present 恢复 |
| 整表 retarget shadow→host | **失败**：c2u UNIMPL，code=0xFFFFFFFF |
| **只 heal shadow gfx 槽** | DrawRect FP 正确，且 text 对象完好 |

#### 当前代码策略

```text
0x2F4494 入口 PC=LR（跳过 chrome）
synth/ui_hooks：heal shadow@0x281EFC 的 gfx FP（保留 c2u 等）
旁路 dump：[JJFB_CTX] tbl / fp@+1E8
```

### 4.3b v33：0x11F00 字素（已推进）

```text
code@0x2A845C: +0x14=len(20), +0x18="CYFmhdNmS1roRgroRa=="
host 画 20×8×16 ASCII → JJFB_DRAW_TEXT_ASCII
DEBUG present ≠ 原生 refresh
```

---

### 4.4 缺图野读 — 已硬化

```text
缺 vmright!…@vmimage.bmp / taskbutton!… 等
旧：r4 毒化 → UC_ERR_READ_UNMAPPED @0x83254 → exit(1)
现：UNMAPPED 不 exit；PC 拉到 stopAddr；打 [JJFB_UNMAP]
最新 present 跑：Failed/UNMAP ≈ 0（nop chrome + 无坏 drawFP seed）
```

`vmimage.mrp` 在 `mythroad\gwy\jjfbol\`；`wy_jiao*` 字符串在 `jjfb.mrp` / `dsm_gm.mrp` 内。  
加载路径/当前打开的 MRP 仍可能不对——以后修资源，但不是当前 present 主阻塞。

---

## 5. 关键改动文件

| 文件 | 改动摘要 |
|------|----------|
| `runtime\vmrp_src_build_v27\vmrp-master\bridge.c` | glyph/ASCII 11F00；shadow gfx heal；CTX dump；nop 2f4494 |
| `runtime\vmrp_src_build_v27\vmrp-master\jjfb_font8x8.inc.c` | 8×8 ASCII 字模 |
| `Cursor_JJFB_v33_*.md` | v33 主文档 |
| 本交接包 | 总览 |

编译部署：

```powershell
$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
cd runtime\vmrp_src_build_v27\vmrp-master
mingw32-make
Copy-Item bin\main.exe ..\..\vmrp_win32\vmrp_win32_20220102\main.exe -Force
cd ..\..\vmrp_win32\vmrp_win32_20220102
.\main.exe
# 日志：.\jjfb_loader_stdout.txt
```

---

## 6. 明确禁止

```text
❌ 假造 mrc_refreshScreen / 把 DEBUG_PRESENT 标成原生 refresh
❌ 回退 loader / 大范围 skip start.mr / 猜 sdk_key
❌ 用 UCRT64 编 32 位 guest 桥
❌ 再把 uptime 写入 0x830/0x834
❌ 再把 drawFP@1510 盲接到 drawBitmap
❌ 永久整段 bypass 2f284c（已改为更精确的 nop 2f4494）
❌ 为“看起来有图”编造不存在的原生 present
```

---

## 7. 下一棒优先级

### P0 — 加深 `0x11F00` 真字素

```text
code 对象不是 C 字符串；需 RE glyph/run 结构
用真实宽度，而不是固定 160x16 占位条
仍用 DEBUG present；等原生 DispUpEx
```

### P1 — 恢复 `2f4494` chrome（可选，非 present 必需）

```text
查清 303d94 的 graphics ctx 为何给出 CODE 区 FP
修 ctx / 表基址，使 r7 = 真 mr_table stub
再搞清 310bb4 ABI（贴图），去掉 nop
```

### P2 — 原生 present

```text
观察 DispUpEx / mrc_refreshScreen 计数
有 buffer 写入后若仍无，再查 refresh gate，仍不要假 refresh
```

### P3 — 资源路径

```text
vmimage.mrp / wy_* 从正确 MRP 读出
失败时返回安全空对象，避免毒指针
```

### 更后

```text
收敛 FORCE state → 自然 state0 / 0x2e48xx UI init
modules → initNetwork → 端口 20000/21002/6009
```

---

## 8. 给下一模型的操作建议

1. 先读本文件 + `Cursor_JJFB_v32_11F00_blit_2f4494_resources.md`  
2. 用 mingw32 编 `bridge.c`，跑 runtime，看 `jjfb_loader_stdout.txt`  
3. 确认仍有 `JJFB_DRAW_TEXT` / `JJFB_DEBUG_PRESENT`  
4. **主攻 0x11F00 code 对象 RE**；chrome 可稍后  
5. 改代码前用 `logs\robotol_ext.bin` + capstone，VA = `file_off + 0x2D8DEC`

---

## 9. 关键地址速查（robotol VA）

| VA | 角色 |
|----|------|
| `0x2E87AC` | UI tick 入口（C44 后） |
| `0x2F284C` | 布局包装 → 2ea180 |
| `0x2EA180` | mode 分发 → 2f4494 |
| `0x2F4494` | chrome 框绘制（**当前 nop**） |
| `0x303D94` | 调图形 FP（曾 blx 0x80278） |
| `0x310BB4` | chrome 贴图循环 |
| `0x2E885A` | 2f284c 返回后 |
| `0x305BF4` | drawText 准备 |
| `0x305C34` / `0x2F2358` | → sendAppEvent |
| `0x11F00` | 图形池 drawText（host 实现中） |
| `0x12340` | 度量（非阻塞） |
| `ERW+0x1510` | drawFP（不 seed） |
| `ERW+0xA64` | chrome bmp 懒加载 flag/ptr |

---

**完。** 下一棒从 P0（`0x11F00` 真字素）开始即可。
