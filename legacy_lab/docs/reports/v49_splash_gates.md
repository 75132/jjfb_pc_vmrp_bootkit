# v49：启动检查三道门

合并自 v48 结论 + v49 运行探针（不替代 `docs/history/` 原文）。

## 一句话

自然路径在 progress 循环后因 **`*(ERW+0xB6C)==0`** 进不了 `0x2EFB06`；即便强行进入，还有 **`*(ERW+0x134D)`** 与 **`*(ERW+0xAC8)`** 两道门。

## 门控

| PC | 条件 | 失败去向 |
|----|------|----------|
| `0x2EFAF0` | `*(ERW+0xB6C)!=0` | 否则看 r4 / `BEQ 0x2EFA18` |
| `0x2EFBA8` | `*(ERW+0x134D)!=0` | `BLE → 0x2EFBF0` 跳过成功段 |
| `0x2EFBFE` | `*(ERW+0xAC8)>0` | 否则 `B → 0x2EFA18`（停在 `0x2EFC00`） |

`0x2EFC58=.word BA0`、`0x2EFC6C=.word AC8` 仍是字面量池；深路径里 **`0x2EFB16` 会 LDR BA0**。

## 探针

| env | 效果 |
|-----|------|
| `JJFB_FORCE_B6C=1` | 进 `0x2EFB06+`，max≈`0x2EFC00` |
| `+ JJFB_FORCE_134D=1` | 进成功段 `0x2EFBAA+` |
| `+ JJFB_FORCE_AC8_GATE=1` | 过 `0x2EFC02+`，随后异常 `@0x2D92A8` |

## 自然写入（扩监视到 ERW+0x1400）

`B6C` / `134D` / `AC8` / `progress_count`：**0 次 guest 写**。

## 下一步

1. 反汇编并 hook `0x305EFC`、`0x31258C` 附近（`0x134D` 字面量）
2. 查谁应创建并挂上 `ERW+0xB6C`（全镜像几乎无 `0xB6C` 写侧字面量）
3. 弄清 `AC8` 的自然生产者（成功段未必写它）

## 相关

- `v48_2EF86C_branch_eventcode.md` / `v48_2EF86C_block_coverage.md` / `v48_2EFC40_tail_disasm.md`
- `v47_progress_writer_startup_check.md`
- 日志：`logs/v49_*`
