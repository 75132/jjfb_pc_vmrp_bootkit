# E8K event-switch 0x30D300 provenance (static)

## Registration (TARGET_OBSERVED from product logs + image)

- plat `0x10102` family `0x1E200` handler Thumb `0x30D301` (even `0x30D300`)
- role=`family_register` host_drain=`no`
- in-module BL callers: **0**
- absolute literal ptrs in ext: **0**
- implication: 0x30D300 is not reached by in-module BL; it is the 0x10102-registered family handler (Thumb 0x30D301). Product only REGISTERs it; host never drains/delivers 0x10102 events into the handler after register.

## Switch / case table

- entry `0x30D300` index=R0 bound=`0x157` table_base=`0x30D324`
- case 310 (0x136) arm 0x30D72E → BL 0x2DFC3C
- case 156 (0x9C) → BL 0x300158
- cases_to_hot: {"0x300158": [156], "0x2DFC3C": [310]}

### Interesting cases

- case `156` (`0x9C`) arm=`0x30DDF4` hot=0x300158 parent=True state=False idle=False bls=['0x300158']
- case `310` (`0x136`) arm=`0x30D72E` hot=0x2DFC3C parent=False state=False idle=False bls=['0x2dfc3c']

## B7D drain

- chain: `0x2F5734 BL → gate@0x305EB8 → 0x305EBE BL → drain@0x2DC80C`
- BL callers of drain: 1
- BL callers of gate: 1

- drain BL `0x305EBE` from fn=`0x305EB8` sole static BL into drain
- gate BL `0x2F5734` from fn=`0x2F5436`

## B7D readers (sample)

- access=`0x2DA63A` fn=`0x2DA624` up=0 in_drain=False
- access=`0x2DC854` fn=`0x2DC848` up=0 in_drain=True
- access=`0x2DC87E` fn=`0x2DC848` up=0 in_drain=True
- access=`0x2DC946` fn=`0x2DC848` up=0 in_drain=True
- access=`0x2DC9A2` fn=`0x2DC984` up=1 in_drain=True
- access=`0x2FE1FE` fn=`0x2FE17E` up=0 in_drain=False
- access=`0x30D294` fn=`0x30D28C` up=0 in_drain=False
- access=`0x30FCDA` fn=`0x30FCAC` up=1 in_drain=False

## Ranked hypotheses (pre-live)

1. `MISSING_APP_INIT_DISPATCH` — 0x10102 family handler 0x30D301 registered but never delivered/drained
1. `EVENT_SWITCH_CALLER_NEVER_REACHED` — no in-module BL; only host/platform delivery of 0x10102 can enter
1. `EVENT_SWITCH_CASE_DERIVED_NEXT_GAP` — case 310→0x2DFC3C and case 156→0x300158 derived; needs correct R0 delivery
1. `MISSING_QUEUE_CONSUMER_TO_DISPATCHER` — B7D drain 0x2DC80C only reached via timer/callback gate; product cold
1. `MISSING_PLATFORM_SIDE_EFFECT_STATE_38` — state still downstream of cold switch/parent path

## BP spec (n=16)

`e:0x30D300,e:0x30D730,e:0x2DFC3C,e:0x2E0E00,e:0x2DC778,p:0x300158,p:0x300714,q:0x2DC80C,q:0x305EB8,q:0x305EBE,u:0x2F5734,u:0x2F5404,e:0x30DDF4,e:0x30D72E,u:0x30D24C,u:0x30D28C`

