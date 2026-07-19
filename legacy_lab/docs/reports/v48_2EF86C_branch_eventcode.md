# v48 结论：不是字面量 writer，是 r4 早退；event_code 扫描未解锁

## 一句话

**`0x2EF86C` 在 progress 循环后因 `CMP r4,#0 / BEQ` 于 `0x2EFAF4` 退出；`0x2EFC58/6C` 只是字面量池。强制进 `0x2EFC40` 也不写 progress/AC8。`r1=0..0x30` 扫描全部仍停在 `0x2EFAF4`。**

---

## 1. 覆盖图

| | |
|--|--|
| max_pc（自然） | **0x2EFAF4** |
| 0x2EFC40 | **0 hits** |
| 真正跳过点 | **0x2EFAF2 CMP r4,#0 ; 0x2EFAF4 BEQ exit** |

详见 `v48_2EF86C_block_coverage.md`。

---

## 2. 反汇编纠正（相对 v47）

```text
0x2EFC58 = .word 0xBA0   (pool)
0x2EFC6C = .word 0xAC8   (pool)
0x2EFC54 = B → 0x2EFCC8  (跳过 pool)
```

**不能再把 imm 地址当成 STR writer。**  
见 `v48_2EFC40_tail_disasm.md`。

---

## 3. FORCE_2EFC_TAIL

```text
divert 0x2EFAE8 → 0x2EFC40 → 执行到 ~0x2EFCFE
progress/AC8 写入 = 0
```

说明该入口在当前寄存器状态下**不是**进度写入例程（或缺前置状态）。

---

## 4. event_code scan

`r1∈{0..8,0x10..0x15,0x18,0x1A,0x20,0x24,0x28,0x30}`：

```text
全部 max_pc≈0x2EFAF4，tail/prog/ac8/str = 0
```

**单改 r1 不能越过 r4 门闩。**  
见 `v48_eventcode_scan.md`。

---

## 5. 0x10140 / 0x306305

```text
handler=0x306305 → 306344/30662C → 2EF86C
稳定 event_code r1=0x13
```

Timer 在喂 paint/tick；splash 画完 loading 后因 **r4==0** 返回。

---

## 6. 下一步（v49）

1. **盯死 r4**：在 `0x2EFAF0` 前记录谁写 r4；自然非 0 的条件。  
2. 对 **r4≠0 路径**（`0x2EFB00+`）做覆盖，找真正 `STR` → `BA0+0x2C` / `AC8`。  
3. 查 pool 的 **LDR 引用点**（不是 pool 地址本身）。  
4. 不再手动 progress driver；不再把 `0x2EFC58` 当 writer。

---

## 运行

```powershell
.\RUN_V48_2EF86C_COVERAGE.ps1
.\RUN_V48_2EF86C_COVERAGE.ps1 -ForceTail 1
.\RUN_V48_EVENTCODE_SCAN.ps1 -From 0 -To 0x30
```
