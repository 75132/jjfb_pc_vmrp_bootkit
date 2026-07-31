# E8I dispatcher parent census + state provenance (static)
code_base=0x2D8DF4
parent=0x300158 callers=85
state_offset=2256 (R9+(0x800+0xD0); audit-safe spelling of former state/ui word)

## Bucket counts

```
{
  "constant_event_code": 81,
  "other": 1,
  "propagated_or_unknown": 3
}
```

## Priority callers (interesting R0 / init/resource/network)

- BL `0x2DC7D4` fn=`0x2DC778` bucket=`constant_event_code` r0_kind=`const_movs` const=`20` strs=[]
- BL `0x2DFDD6` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['q@h/', 'HxD/']
- BL `0x2DFE58` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['q@h/', 'HxD/']
- BL `0x2DFE68` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['q@h/', 'HxD/']
- BL `0x2DFE8E` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['q@h/', 'HxD/']
- BL `0x2DFEF8` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['HxD/']
- BL `0x2DFF18` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['HxD/']
- BL `0x2DFF3E` fn=`0x2DFC3C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=['HxD/']
- BL `0x2E057A` fn=`0x2E057A` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E0664` fn=`0x2E0664` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E06DE` fn=`0x2E06DE` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E074C` fn=`0x2E074C` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E0FB2` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1008` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1048` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E10BA` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E10EC` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`20` strs=[]
- BL `0x2E1126` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1186` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1234` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1266` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E12E0` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E144E` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E1474` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]
- BL `0x2E14B8` fn=`0x2E0E00` bucket=`constant_event_code` r0_kind=`const_movs` const=`18` strs=[]

## Dispatcher cases that can lead toward 0x30103C

- 0x300744: CMP r0, #38 → Bcond(0) tgt=0x300816 lead=True

## State-word writer candidates (LDR =2256 + STR*)
count=96

- store@`0x2DA790` ldr@`0x2DA778` fn=`0x2DA68C` width~4 no_const_38_seen_in_fn
- store@`0x2DC644` ldr@`0x2DC63A` fn=`0x2DC620` width~4 no_const_38_seen_in_fn
- store@`0x2DC94E` ldr@`0x2DC942` fn=`0x2DC80C` width~4 no_const_38_seen_in_fn
- store@`0x2DCE66` ldr@`0x2DCE4A` fn=`0x2DCE04` width~4 no_const_38_seen_in_fn
- store@`0x2DD440` ldr@`0x2DD42C` fn=`0x2DD068` width~4 no_const_38_seen_in_fn
- store@`0x2DD478` ldr@`0x2DD472` fn=`0x2DD068` width~4 no_const_38_seen_in_fn
- store@`0x2DD512` ldr@`0x2DD50C` fn=`0x2DD068` width~4 no_const_38_seen_in_fn
- store@`0x2DD51E` ldr@`0x2DD516` fn=`0x2DD068` width~4 no_const_38_seen_in_fn
- store@`0x2DD640` ldr@`0x2DD63C` fn=`0x2DD068` width~1 no_const_38_seen_in_fn
- store@`0x2DD736` ldr@`0x2DD722` fn=`0x2DD6A0` width~1 no_const_38_seen_in_fn
- store@`0x2E052C` ldr@`0x2E0526` fn=`0x2E0526` width~4 no_const_38_seen_in_fn
- store@`0x2E0AE4` ldr@`0x2E0ADA` fn=`0x2E0ADA` width~4 no_const_38_seen_in_fn
- store@`0x2E0E3C` ldr@`0x2E0E16` fn=`0x2E0E00` width~4 no_const_38_seen_in_fn
- store@`0x2E2488` ldr@`0x2E2464` fn=`0x2E2458` width~4 no_const_38_seen_in_fn
- store@`0x2E2F60` ldr@`0x2E2F58` fn=`0x2E2F58` width~4 possible_const_38_in_fn
- store@`0x2E3254` ldr@`0x2E3236` fn=`0x2E3236` width~1 possible_const_38_in_fn
- store@`0x2E346E` ldr@`0x2E3466` fn=`0x2E3466` width~4 no_const_38_seen_in_fn
- store@`0x2E39EC` ldr@`0x2E39E4` fn=`0x2E39E4` width~4 possible_const_38_in_fn
- store@`0x2E3A04` ldr@`0x2E39FC` fn=`0x2E39FC` width~4 possible_const_38_in_fn
- store@`0x2E3F8C` ldr@`0x2E3F84` fn=`0x2E3F84` width~4 no_const_38_seen_in_fn
- store@`0x2E3FCC` ldr@`0x2E3FC6` fn=`0x2E3FC6` width~4 no_const_38_seen_in_fn
- store@`0x2E55B8` ldr@`0x2E55B0` fn=`0x2E5428` width~4 no_const_38_seen_in_fn
- store@`0x2E6986` ldr@`0x2E6976` fn=`0x2E68F0` width~4 no_const_38_seen_in_fn
- store@`0x2E69C8` ldr@`0x2E69C0` fn=`0x2E68F0` width~4 no_const_38_seen_in_fn
- store@`0x2E796E` ldr@`0x2E7966` fn=`0x2E7934` width~4 no_const_38_seen_in_fn
- store@`0x2E83C6` ldr@`0x2E83A0` fn=`0x2E8384` width~4 no_const_38_seen_in_fn
- store@`0x2E87F2` ldr@`0x2E87D8` fn=`0x2E87B4` width~4 no_const_38_seen_in_fn
- store@`0x2E9968` ldr@`0x2E9952` fn=`0x2E993C` width~4 no_const_38_seen_in_fn
- store@`0x2EE878` ldr@`0x2EE852` fn=`0x2EE650` width~4 no_const_38_seen_in_fn
- store@`0x2EE8CA` ldr@`0x2EE8AE` fn=`0x2EE650` width~4 no_const_38_seen_in_fn

## Ranked hypotheses (pre-live)

1. `MISSING_APP_INIT_DISPATCH` — 0x300158 never entered; many callers look init/event-ish but product cold
1. `MISSING_PLATFORM_SIDE_EFFECT_STATE_38` — state word stays 0; arm to 0x30103C needs value 38
1. `MISSING_RESOURCE_READY_DISPATCH` — resourceish caller bucket present statically
1. `MISSING_NETWORK_READY_DISPATCH` — networkish bucket present; only claim if live caller proves
1. `MISSING_QUEUE_CONSUMER_TO_DISPATCHER` — 10165 sets FE8/B7D but never proven to call 0x300158

## BP CSV (priority)

`0x300158,0x3002C0,0x300714,0x30103C,0x3020C8,0x302340,0x302362,0x2DC7D4,0x2DFDD6,0x2DFE58,0x2DFE68,0x2DFE8E,0x2DFEF8,0x2DFF18,0x2DFF3E,0x2E057A,0x2E0664,0x2E06DE,0x2E074C,0x2E0FB2,0x2E1008,0x2E1048,0x2E10BA,0x2E10EC,0x2E1126,0x2E1186,0x2E1234,0x2E1266,0x2E12E0,0x2E144E,0x2E1474,0x2E14B8,0x2E1670,0x2E17DA,0x2E183E,0x2E1860,0x2E187A`

