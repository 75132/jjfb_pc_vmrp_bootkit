# D5b — command dispatcher xref

- cmd_dispatcher_va: `0x2E3A18` (off `0xF6C4`)
- evidence: TARGET_OBSERVED (D5 probe off=0xF6C4)

## Direct branches

- `bl` from `0x2E3ADE`
- `bl` from `0x2E3C58`
- `bl` from `0x2E79B8`

## Literal pools

(none)

## Char comparisons near dispatcher (manual TARGET_OBSERVED)

```text
B / H / I / P / S / c / s / p
```

Looks like a protocol/command stream parser, not keepalive timer.

## Handler reachability (heuristic)

- event `0x10600` handler `0x2E74AD` → cmd_disp: yes
- event `0x10601` handler `0x2E03E1` → cmd_disp: yes
- event `0x10602` handler `0x2E0421` → cmd_disp: yes
- event `0x10603` handler `0x2E0359` → cmd_disp: yes
- event `0x10604` handler `0x2E0445` → cmd_disp: yes
- event `0x10605` handler `0x2E0361` → cmd_disp: yes
- event `0x10606` handler `0x2DFC61` → cmd_disp: yes
- event `0x1060A` handler `0x2DFC59` → cmd_disp: yes
- event `0x10608` handler `0x2DF699` → cmd_disp: yes
- event `0x10609` handler `0x2E0405` → cmd_disp: yes
- timer `0x2E7754` → cmd_disp: yes

