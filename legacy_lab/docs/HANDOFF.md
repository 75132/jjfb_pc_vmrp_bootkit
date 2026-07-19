# 当前入口（整理后）

> **跑：** 根目录 `RUN_JJFB.ps1`（实现：`RUN_V71_PRESENT_COALESCE.ps1`）  
> **规则：** `.cursor/rules/Rules.mdc` · skill：`.cursor/skills/jjfb-gwy-launcher-shim/SKILL.md`  
> **事实：** [`LOCKED_FACTS.md`](LOCKED_FACTS.md) · **索引：** [`INDEX.md`](INDEX.md)
>
> 主线：GWY Launcher Shim（cfg index=36、`mythroad/320x480` 资源根、guest LCD 240×320、`startGame/runapp`）。  
> 禁止 force UI / AC8 / progress / host overlay；不改 `jjfb.mrp`。  
> 旧 V63–V70 runner / 日志 / 报告：`archive/*/2026-07-17_jjfb_clean/`。
>
> 下文「v63 覆盖通知」与「v49」正文均为历史，勿当当前任务单。

---

# ⚠️ 路线覆盖通知（v63 · 历史）

> 本文后半部分是 v49 UI gate 历史记录，**不再是当前主线**。  
> 历史路线文档：`README/01_HANDOFF_CONTEXT/LATEST_DIRECTION_v63.md`  
> 历史 runner 已归档：`archive/runners/2026-07-17_jjfb_clean/`
>
> 主线：GWY Launcher Shim（cfg index=36、`mythroad/320x480/gwy` 资源根、`startGame/runapp`）。禁止 force UI/AC8/progress/host overlay。

---

# JJFB / PC-vmrp 交接（v49 · 历史）

生成：2026-07-17  
主代码：`runtime/vmrp_src_build_v27/vmrp-master/bridge.c`  
历史 runner：`scripts/runners/RUN_V49_R4_GATE.ps1`（当前请用根目录 `RUN_JJFB.ps1`）

---

## 0. 一句话状态

```text
目标：在 ui_mode=0x45 下让启动检查 UI 自然跑完（slogo→loadingbar→检查网络/更新）。
卡点：0x2EF86C 深路径被三道门挡住；自然运行无 guest 写 B6C/134D/AC8/progress。
下一步：找出谁该写 ERW+0xB6C、+0x134D、+0xAC8（看 0x305EFC / 0x31258C）。
```

**不要：** host overlay、progress driver 当方案、跳过 0x45 追登录、UI 抛光、打包 zip。

---

## 1. 目标与路径

```text
start.mr → mrc_loader.ext → robotol.ext → modules → network
```

| 项 | 值 |
|----|-----|
| 项目根 | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit` |
| 源码 | `runtime\vmrp_src_build_v27\vmrp-master` |
| 运行时 | `runtime\vmrp_win32\vmrp_win32_20220102` |
| 编译 | MSYS2 **mingw32 i686**（禁 UCRT64）；改后先删 `bridge.o` |
| 屏 | 240×320 RGB565；轴修复 `2F9968→240`，`2F995C→320` |
| 日志 | runtime CWD `jjfb_loader_stdout.txt`（勿管道重定向） |

---

## 2. 已锁定（勿重开）

- `0x10134` 返回 **像素指针**；禁 fail-open / 禁返回 object
- FORCE splash：`ui_mode → 0x45`（`JJFB_FORCE_SPLASH_NUDGE=45`）
- EAGER blit 默认 **关**；`DEBUG_PRESENT` ≠ 原生 DispUpEx
- BA0（ERW 常 `0x2B1850`）：`+0x20` bar，`+0x24` textbar，`+0x28` loadingbar，`+0x2C` **progress_count**
- `0x2EFC58/6C` 是字面量池（BA0/AC8），**不是** STR writer
- 调用链：`0x10140 → 0x306305 → 0x306344 → cmp #0x45 → 0x30662C bl 0x2EF86C`；自然 `r1=0x13`

---

## 3. v49 门控链（当前主线）

progress 循环结束后：

```text
LDR r0, *(ERW+0xB6C)
CMP r0,#0
BNE 0x2EFB06          ; 主深路径
CMP r4,#0
BEQ 0x2EFA18          ; r4 次要；自然 r4 一直为 0

; 深路径末尾：
LDR *(ERW+0x134D)
BLE 0x2EFBF0          ; ==0 跳过成功段
…
LDR *(ERW+0xAC8)
BGT 0x2EFC02          ; AC8>0 才继续
B   0x2EFA18          ; AC8==0 退出（自然 max_pc=0x2EFAF4 / 探针到 0x2EFC00）
```

| 槽位 | 含义 | 自然写入 |
|------|------|----------|
| `ERW+0xB6C` | 深路径门 | **0**（字面量全镜像仅 1 处读者） |
| `ERW+0x134D` | 成功段门 | **0**（xref `@0x305EFC` `@0x31258C`） |
| `ERW+0xAC8` | 过 `0x2EFC00` | **0** |
| `BA0+0x2C` | progress_count | **0**（driver 能画 bar，但不够） |

探针（仅实验）：`JJFB_FORCE_B6C` / `JJFB_FORCE_134D` / `JJFB_FORCE_AC8_GATE`  
三联可到 `0x2EFC02+`，随后 `UC_ERR_EXCEPTION @0x2D92A8`（假状态）。

---

## 4. 常用环境变量

```text
JJFB_FORCE_SPLASH_NUDGE=45
JJFB_FORCE_UI_MODE=45
JJFB_10134_RET=pixels
JJFB_10134_EAGER_BLIT=0
JJFB_AXIS_FIX=1
JJFB_COLORKEY=auto
JJFB_SPLASH_HOST_BLIT=1
JJFB_PROGRESS_DRIVER=off
JJFB_AC8_MODE=natural
JJFB_FORCE_B6C=0
JJFB_FORCE_134D=0
JJFB_FORCE_AC8_GATE=0
JJFB_FORCE_R4=0
JJFB_FORCE_2EFC_TAIL=0
```

---

## 5. 文档入口

| 文件 | 用途 |
|------|------|
| **本文件** | 唯一当前交接 |
| `docs/INDEX.md` | 文档索引 |
| `docs/LOCKED_FACTS.md` | 锁定事实速查 |
| `docs/reports/` | 分版结论（v43–v49） |
| `docs/history/` | 旧 Cursor brief / tip / 旧 handoff |
| `scripts/runners/` | V27–V48 历史 runner |
| `archive/` | 旧 pack / zip / 早期 README |

运行当前探针：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_V49_R4_GATE.ps1
```
