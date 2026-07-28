# P6–P7 Boot Successor Closure — stage notes

## Goal

Keep Layer-1 first frame; find why resource #6 never appears after the shared
`caller_lr=0x2D93D1` UI builder completes five BMPs.

## Delivered in this round

| Phase | Status |
|-------|--------|
| P6-0 | `boot_successor_trace` → timeline / PC histogram / verdict under `reports/p6_*` |
| P6-1 | `JJFB_304BF0_SIDE_EFFECT_AUDIT=1` logs; `JJFB_304BF0_ENTRY_COMPLETE=0` A/B native continue |
| P6-2 | `note_pixels*` no longer enqueue zero pixels; `NOTE_PIXELS_LEGACY_CALLS` counter |
| P7-0 | `reports/p7_family_event_abi.csv` on family handler deliver |
| P7-1 | **Not applied** — no ABI mutation until producer/registration/consumer closes |

## Successor Gate (not yet claimed)

```text
unique resource count >= 6
6th request must be guest-natural
prefer caller_lr != 0x2D93D1 or active_package nonempty
```

## Dense PC capture

```text
JJFB_BOOT_SUCCESSOR_TRACE=1
```

Light markers (resource 1–5, family events) are default ON.
