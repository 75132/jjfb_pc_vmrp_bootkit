# JJFB / PC-vmrp 交接包（发给 GPT · v33）

生成时间：2026-07-17  
用途：继续推进 PC-vmrp 跑 JJFB；**勿从零猜测、勿回退已稳定层**。

同内容文件：`00_HANDOFF_LATEST.md` / `HANDOFF_LATEST_FOR_GPT.md` / `logs/HANDOFF_LATEST_FOR_GPT.md`

---

## 0. 一句话状态

```text
present 中层已通；0x11F00 已按 code 对象 glyph 数画 8×16 ASCII 字素（DEBUG present）；
2f4494 仍 nop；shadow mr_table@0x281EFC 的 DrawRect 等 gfx FP 已 heal 到 host stubs；
禁止整表 retarget 到 host mr_table（会打爆 c2u→code=-1）；
原生 DispUpEx / mrc_refreshScreen 仍为 0。
下一步 = 有限放行 chrome 试 310bb4 / 字素语义加深 / 资源路径 / 等原生 present。
```

**绝对不要：**
- 编造 `sdk_key`
- 假冒 `mrc_refreshScreen` / 把 `DEBUG_PRESENT` 说成原生 refresh
- 回退 loader / robotol / timer
- 用 UCRT64
- 把 `*(ext_base)` 整表换成 host `mr_table`
- 再把 uptime 写入 `0x830/0x834`
- 再盲 seed `drawFP@1510`

---

## 1. 目标

```text
start.mr → mrc_loader.ext → robotol.ext → modules → network (20000 / 21002 / 6009)
```

当前卡在 **UI present / 绘制语义**，不是 loader、不是 sdk_key、不是 timer 基础。

---

## 2. 路径与环境（已验证，勿改）

| 项 | 路径 / 结论 |
|----|-------------|
| 项目根 | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit` |
| 源码 | `runtime\vmrp_src_build_v27\vmrp-master`（主改 `bridge.c`） |
| 字模 | `runtime\vmrp_src_build_v27\vmrp-master\jjfb_font8x8.inc.c` |
| 运行时 | `runtime\vmrp_win32\vmrp_win32_20220102` |
| 最佳 MRP | `logs\jjfb_801diag_only6_skip0_v27.mrp` |
| robotol dump | `logs\robotol_ext.bin`（VA base **`0x2D8DEC`**） |
| 编译 | **MSYS2 mingw32 i686**，`C:\msys64\mingw32\bin`，**禁止 UCRT64** |
| 日志 | runtime CWD 的 `jjfb_loader_stdout.txt`（`freopen`；管道重定向会空） |

### 必读文档

| 文件 | 内容 |
|------|------|
| **本文件** | 总交接 |
| `Cursor_JJFB_v33_11F00_glyph_chrome_native_present.md` | v33 详细说明 + 实施进度 |
| `Cursor_JJFB_v32_11F00_blit_2f4494_resources.md` | present 中层 / 2f4494 定性 |

### 证据日志

| 文件 | 说明 |
|------|------|
| `logs\v33_shadow_heal_stdout.txt` | **最新成功**：heal + ASCII glyph + code 完好 |
| `logs\v33_glyph_ascii_present_stdout.txt` | 多字素 ASCII 首通 |
| `logs\v33_ctx_retarget_stdout.txt` | **反例**：整表 retarget→c2u UNIMPL→code=-1 |
| `logs\v32_nop4494_present_stdout.txt` | nop chrome 后 present 首通 |

---

## 3. 已稳定层（禁止回退）

1. mingw32 + unicorn + 低地址 map  
2. loader：`mrc_loader.ext` → 801 → `robotol.ext`  
3. host `mrc_init(0) ret=0`（合成 `mrc_extChunk` + `sendAppEvent`）  
4. timer / `0x10140` / heap gates / `mr_registerAPP`  
5. refresh gate C44 via `0x2FC8B8`；FORCE state 0→1  
6. 屏参 seed：`824w=240 828y=0 830h=320 834w=240`（**不是 clock**）  
7. present 中层：`305bf4 → 0x11F00 → DEBUG present`  
8. `2f4494` 精确 nop（不是整段 bypass `2f284c`）  
9. **v33** shadow gfx heal；0x11F00 多字素 ASCII  

### 屏参（禁止再污染）

```text
0x830 = 屏高（getter 2f9968）
0x834 = 屏宽（getter 2f995c）
禁止写 uptime
```

### drawFP@1510

```text
自然值常为 0x270F；禁止再 seed 到 drawBitmap（ABI 不匹配）
```

---

## 4. v33 关键技术结论

### 4.1 0x11F00 code 对象（已解析最小集）

```text
app = 7
code = 富文本/glyph 对象，不是 C 字符串
  +0x14 = payload length（观察到 20）
  +0x18 = payload（"CYFmhdNmS1roRgroRa==" base64-like ASCII）
p0:
  +0x00 = y (int16)
  +0x02 = x (int16)
  +0x2C = RGB888

host 行为：
  按 len 画 N 个 8×16；ASCII 用内置字模
  JJFB_DRAW_TEXT / JJFB_DRAW_TEXT_ASCII / JJFB_DEBUG_PRESENT
  base64 decode 后 printable≈46%，暂仍画 raw ASCII
```

证据摘要：

```text
[JJFB_CODE_OBJ] ptr=0x2A845C ... len=+14:20 ascii="CYFmhdNmS1roRgroRa=="
[JJFB_DRAW_TEXT] glyphs=20 ascii=20 block=0 160x16
[JJFB_DEBUG_PRESENT] ... (host debug; DispUpEx/mrc_refreshScreen untouched)
```

### 4.2 2f4494 / 303d94 graphics ctx

```text
链路：2e87ac → 2f284c → 2ea180(mode=0xa) → 2f4494 → 303d94 → blx r7
303d94: tbl = *(ext_base=0x2D8DEC) = shadow@0x281EFC
         r7 = *(tbl + 0x1E8)

旧：*(shadow+0x1E8) = 0x80278（CODE 垃圾）→ emu 停 stopAddr
现：jjfb_heal_shadow_gfx 写入 host DrawRect stub 0x2801EC
    2f4494 仍 nop（放行后还会卡 310bb4 / 坏 h）
```

**关键反例：**

```text
把 *(ext_base) 整表改成 host mr_table=0x280004
→ c2u/sprintf/strncpy 变 UNIMPL
→ text code=0xFFFFFFFF
→ 字素对象消失
正确做法：保留 shadow，只 heal gfx 槽（0x74/0x1D8..0x200）
```

### 4.3 present 区分（必须保持）

```text
JJFB_DEBUG_PRESENT > 0     ← host 调试看图
DispUpEx = 0               ← 原生尚未出现
mrc_refreshScreen = 0      ← 原生尚未出现
```

---

## 5. 关键地址速查（robotol VA = file_off + 0x2D8DEC）

| VA | 角色 |
|----|------|
| `0x2E87AC` | UI tick（C44 后） |
| `0x2F284C` | 布局包装 → 2ea180 |
| `0x2EA180` | mode 分发 → 2f4494 |
| `0x2F4494` | chrome 框（**当前 nop**） |
| `0x303D94` | 调图形 FP（tbl+0x1E8） |
| `0x310BB4` | chrome 贴图循环（放行后主风险） |
| `0x2E885A` | 2f284c 返回后 |
| `0x305BF4` / `0x305C34` / `0x2F2358` | → 0x11F00 |
| `0x11F00` | drawText/glyph（host 实现中） |
| `0x12340` | 度量（非阻塞） |
| `0x2D8DEC` | robotol ext_base / GOT 槽 → shadow tbl |
| `0x281EFC` | mythroad shadow mr_table 副本 |
| `0x280004` | host mr_table |
| `ERW+0x1510` | drawFP（不 seed） |

---

## 6. 编译部署

```powershell
$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
cd C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_src_build_v27\vmrp-master

# MSYS make 的 "if not exist" 可能炸；推荐手动：
gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -c bridge.c -o bridge.o
gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -o ./bin/main `
  network.o fileLib.o vmrp.o utils.o rbtree.o bridge.o memory.o main.o `
  ./windows/unicorn-1.0.2-win32/unicorn.lib `
  -lpthread -lm -lws2_32 -lmingw32 -mconsole `
  -L./windows/SDL2-2.0.10/i686-w64-mingw32/lib/ -lSDL2main -lSDL2

Copy-Item bin\main.exe ..\..\vmrp_win32\vmrp_win32_20220102\main.exe -Force
cd ..\..\vmrp_win32\vmrp_win32_20220102
.\main.exe
# 日志：.\jjfb_loader_stdout.txt
```

成功标志（节选）：

```text
[JJFB_CTX] heal(...) shadow@0x281EFC +0x1E8 0x80278 -> 0x2801EC
[JJFB_CODE_OBJ] ... ascii="CYFmhdNmS1roRgroRa=="
[JJFB_DRAW_TEXT] glyphs=20 ascii=20
[JJFB_DRAW_TEXT_ASCII] ...
[JJFB_DEBUG_PRESENT] ... (host debug; DispUpEx/mrc_refreshScreen untouched)
[JJFB_CTX] chrome-bypass dump ... fp@+1E8=0x2801EC
```

---

## 7. 下一棒优先级

### P0 — 字素语义

```text
payload 编码/字库/富文本 run；中文可先 block
不要只停在 base64 ASCII 占位可读
```

### P1 — 有限放行 chrome

```text
shadow DrawRect 已 heal；2f4494 仍 nop
短时取消 nop，观察 310bb4 ABI / bmp 对象 / 坏 h
每次只放一个子调用；失败立刻回 nop
```

### P2 — 原生 present

```text
谁该调 DispUpEx？勿 fake mrc_refreshScreen
DEBUG present 可保留作调试显示
```

### P3 — 资源路径

```text
vmimage.mrp / wy_jiao* / name!w!h.bmp
失败返回安全空对象，勿毒指针
```

---

## 8. 给下一模型的操作建议

1. 读本文件 + `Cursor_JJFB_v33_*.md`  
2. 用 mingw32 编跑，确认 `JJFB_DRAW_TEXT_ASCII` + `heal ... 0x2801EC`  
3. **不要**整表 retarget mr_table  
4. 主攻：字素语义 **或** 有限放行 2f4494→310bb4  
5. RE 用 `logs\robotol_ext.bin` + capstone，VA = `file_off + 0x2D8DEC`

---

**完。** 当前最低成功已达成（多字素 ASCII + shadow DrawRect heal）；下一棒从 P0 或 P1 选一条推进即可。
