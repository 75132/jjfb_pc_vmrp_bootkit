# E8E handler registry map (static trampoline resolve)

| code | handler | trampoline_target | host_drain | role |
|------|---------|-------------------|------------|------|
| 0x10140 | 0x30630D | (body/not thin) | yes_lifecycle | lifecycle_period |
| 0x10165 | 0x30D2F9 | 0x30D24C | no_unless_probe_or_drain_order | enqueue_event |
| 0x10162 | 0x30D249 | (body/not thin) | no | near_enqueue_sibling |
| 0x10102 | 0x30D301 | (body/not thin) | no | family_register |

Notes:
- 0x10165 trampoline 0x30D2F8: PUSH; BL 0x30D24C; POP.
- 0x10162 handler 0x30D249 is immediately before enqueue core 0x30D24C.
- Product loop drains only 0x10140 unless E8E drain-order env enables 0x10165 fire.
