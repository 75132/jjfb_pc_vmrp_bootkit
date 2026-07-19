# Stage E8E — Derive 0x10165 Enqueue ABI / Real Event Payload

## Verdict

`EVENT_ABI_DERIVED_NEXT_GAP`

## Gates

| Gate | Result |
|------|--------|
| configure/build | ok (launcher_core + vmrp gwy) |
| unit tests | fixture-env failures only (unchanged; not E8E regressions) |
| static disasm 0x30D24C | yes |
| abi_inference.md / .json | yes |
| callsite_abi_samples.md | yes (1 BL: trampoline only) |
| handler_registry_map.md | yes |
| FE8 watch armed | yes |
| FE8 write seen | yes (`writer_pc=0x30D262`) |
| live HANDLER_MAP 10165 | yes (`0x30D2F9` → `BL_0x30D24C`) |
| event probe done | yes (`R0_EVENTCODE_2 ok=1`) |
| drain-order B | yes (10140 then 10165) |
| drain-order A | yes (10165 then 10140; flags still zero) |
| idle C44/C9D/CF5 transition | no |
| DRAW | no |
| audit_launcher_core | ok / findings=[] |
| jjfb.mrp SHA-256 | unchanged `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` |

## Live probe (TARGET_OBSERVED)

Candidate `R0_EVENTCODE_2` (r0=2, r1=0), handler `0x30D2F9`, r9=`0x2B1858`:

- `fe8_changed=1` — `R9+0xFE8` stores helper `0x3046A8` return (`STR` at `0x30D262`)
- `b7d_after=1` — short-path status byte `R9+0xB7D`
- `flags_still_zero=1` — `C44/C9D/CF5` remain 0 after probe and through tick 40
- Drain A vs B does **not** unlock idle flags → not `10165_DRAIN_ORDER_REQUIRED`

## Static ABI summary (TARGET_OBSERVED)

- Entry Thumb `0x30D24D` / body `0x30D24C`: save R0→r4, R1→r6; `BL 0x3046A8`; store helper ret at `R9+0xFE8`
- Queue base `R9+0x7D8` fields `+0x24` / `+0x6C`; short path `STRB #1` at `R9+0xB7D`
- Long path (depth≤0) forwards saved R0 into plat `0x101AB` via `BL 0x304558` with `r3=2`
- Only trampoline BL inside robotol (`0x30D2F8` → `0x30D24C`); delivery is platform `0x10165`
- Enqueue core does **not** write idle flags `C44/C9D/CF5`

## Next gap

ABI of the enqueue core is derived; FE8 is explained as helper-return / queue token, not the idle unlock. Next discriminating work is finding who sets `C44/C9D/CF5` (likely a different event/source than a bare `0x10165` fire with small event codes).

## Evidence class

- Disasm / FE8 writer PC / B7D side-effect: TARGET_OBSERVED
- Semantic event names (APP_START / NETWORK_READY / …): HYPOTHESIS — not claimed

## Artifacts

- `out/e8e_tmp/handler_30D24D_disasm.txt`
- `out/e8e_tmp/abi_inference.json` / `abi_inference.md`
- `out/e8e_tmp/callsite_abi_samples.md`
- `out/e8e_tmp/handler_registry_map.md`
- `out/e8e_tmp/probe_candidates.json`
- `logs/stage_e8e_jjfb_stdout.txt`
- `RUN_E8E_EVENT_ABI.ps1`
