# D5b — cfg gate / cfg-open xref

evidence: TARGET_OBSERVED static

| site | role | bl callers | b/bcond | lit pool | nearest_string |
|---|---|---|---|---|---|
| `0x2D5E4C` | cfg_open_a | 0x2e11de | — | — | `IIDHm` |
| `0x2D5E5C` | cfg_open_b | 0x2d829a, 0x2e171e | — | — | `IIDHm` |
| `0x2D5E6C` | cfg_open_c | 0x2dab90 | — | — | `IIDHm` |
| `0x2D5F14` | cfg_open_d | 0x2d9654 | — | — | `IyDJ` |
| `0x2D7C80` | cfg_gate | 0x2e39c6 | — | — | `Kl"KD` |
| `0x2D7CE4` | cfg_gate_mid | 0x2d9c72 | 0x2d7d90:b, 0x2d7ec8:b, 0x2d81e8:b, 0x2d8216:b | — | `Kl"KD` |
| `0x2D829A` | cfg_callsite_b | — | — | — | `` |
| `0x2D9654` | cfg_callsite_d | — | — | — | `IIDH` |
| `0x2D9CBC` | gate_cmp_r0_0xC | — | — | — | `Kx"KD` |
| `0x2DAB90` | cfg_callsite_c | — | — | — | `IIDH` |
| `0x2E0F5C` | cfg_parent_a | 0x2e13d6 | — | — | `\13h` |
| `0x2E11DE` | cfg_callsite_a | — | — | — | `cJdIJD` |
| `0x2E1520` | cfg_parent_b | 0x2e183e | — | — | `L hC` |
| `0x2E39C4` | cfg_wrap | — | — | — | `M}D@` |
| `0x2E3A18` | cmd_disp | 0x2e3ade, 0x2e3c58, 0x2e79b8 | — | — | `IyDB` |

## Gate note

- `0x2D9CBC` contains `cmp r0, #0xC` (12 = napptype) in surrounding function (D4).
- `cfg_wrap` (`0x2E39C4`) had sole `bl` to `cfg_gate` in prior audit; if callers empty, may be dead or table-driven.

