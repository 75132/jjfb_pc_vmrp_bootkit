# E8J caller upstream reachability (static)
code_base=0x2D8DF4
parent=0x300158 bl_callers=85 enclosing_fns=50
bp_count=111

## Hot clusters (E8I-named)

### fn `0x2E0E00` .. `0x2E0E32` parent_bls=21 upstream=2
- r0_const_counts={'18': 20, '20': 1}
- touches={'FE8': False, 'B7D': False, '7D8': False, 'state': True} markers=['state_word']
- strings=[]
- upstream callers (sample):
  - BL `0x3013B8` from fn=`0x3013A6` r0=None strs=[]
  - BL `0x3013CE` from fn=`0x3013CA` r0=80948 strs=[]

### fn `0x2DFC3C` .. `0x2DFCAC` parent_bls=11 upstream=7
- r0_const_counts={'18': 11}
- touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- strings=['q@h/', 'HxD/']
- upstream callers (sample):
  - BL `0x2DC650` from fn=`0x2DC620` r0=None strs=[]
  - BL `0x2F52DA` from fn=`0x2F508A` r0=None strs=[]
  - BL `0x300606` from fn=`0x3005B2` r0=0 strs=[]
  - BL `0x30061C` from fn=`0x3005B2` r0=0 strs=[]
  - BL `0x3006EE` from fn=`0x30067A` r0=0 strs=[]
  - BL `0x300704` from fn=`0x30067A` r0=0 strs=[]
  - BL `0x30D730` from fn=`0x30D5E0` r0=None strs=[]

### fn `0x2DC778` .. `0x2DC804` parent_bls=1 upstream=1
- r0_const_counts={'20': 1}
- touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- strings=[]
- upstream callers (sample):
  - BL `0x2DD3D0` from fn=`0x2DD09E` r0=1 strs=[]

## Top enclosing functions by parent_bl_count

- `0x2E0E00` bls=21 up=2 r0={'18': 20, '20': 1} hot=True touches={'FE8': False, 'B7D': False, '7D8': False, 'state': True} markers=['state_word']
- `0x2DFC3C` bls=11 up=7 r0={'18': 11} hot=True touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E7AC0` bls=2 up=0 r0={'18': 2} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2F5436` bls=2 up=0 r0={'464': 1, '20': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': True, 'state': False} markers=['queue_region']
- `0x2FA500` bls=2 up=43 r0={'18': 2} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': True} markers=['state_word']
- `0x302118` bls=2 up=0 r0={'18': 2} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': True} markers=['state_word']
- `0x312D28` bls=2 up=0 r0={'18': 2} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': True} markers=['state_word']
- `0x2DC778` bls=1 up=1 r0={'20': 1} hot=True touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1A8E` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1ADC` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1BC0` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1BF4` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1C10` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1CF6` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]
- `0x2E1E16` bls=1 up=0 r0={'18': 1} hot=False touches={'FE8': False, 'B7D': False, '7D8': False, 'state': False} markers=[]

## Queue consumers (static)

- FE8_readers: 1
- B7D_readers: 8
- 7D8_readers: 59
- FE8_writers: 2
- B7D_writers: 13

### Bridge readers that can call parent cluster

- B7D_reader fn=`0x2DC848` access=`0x2DC854` bl_targets=['0x2e2520'] up=0
- B7D_reader fn=`0x2DC848` access=`0x2DC87E` bl_targets=['0x2e2520'] up=0
- B7D_reader fn=`0x2DC848` access=`0x2DC946` bl_targets=['0x2e2520'] up=0
- 7D8_reader fn=`0x2F5436` access=`0x2F5446` bl_targets=['0x300158'] up=0
- 7D8_reader fn=`0x2F5436` access=`0x2F5484` bl_targets=['0x300158'] up=0
- 7D8_reader fn=`0x2F5436` access=`0x2F557E` bl_targets=['0x300158'] up=0
- 7D8_reader fn=`0x2F5436` access=`0x2F5712` bl_targets=['0x300158'] up=0
- 7D8_reader fn=`0x2F5436` access=`0x2F5760` bl_targets=['0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x30403A` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x304086` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x30424E` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x3042A4` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x304332` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x303FFA` access=`0x3043CA` bl_targets=['0x2f5390', '0x300158'] up=0
- 7D8_reader fn=`0x306322` access=`0x30634C` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x306322` access=`0x306796` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x306322` access=`0x3067B6` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x306322` access=`0x3067DE` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x306322` access=`0x306834` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x306322` access=`0x3068BC` bl_targets=['0x306904'] up=0
- 7D8_reader fn=`0x30D5E0` access=`0x30D5EE` bl_targets=['0x2dfc3c', '0x2fa500', '0x2fb5bc', '0x300158'] up=0
- 7D8_reader fn=`0x30D5E0` access=`0x30D64A` bl_targets=['0x2dfc3c', '0x2fa500', '0x2fb5bc', '0x300158'] up=0

## FE8 readers (detail)

- access@`0x30D268` fn=`0x30D24C` can_call_parent=False is_parent_fn=False up=1 touches_state=False

## B7D readers (detail)

- access@`0x2DA63A` fn=`0x2DA624` can_call_parent=False is_parent_fn=False up=0
- access@`0x2DC854` fn=`0x2DC848` can_call_parent=True is_parent_fn=False up=0
- access@`0x2DC87E` fn=`0x2DC848` can_call_parent=True is_parent_fn=False up=0
- access@`0x2DC946` fn=`0x2DC848` can_call_parent=True is_parent_fn=False up=0
- access@`0x2DC9A2` fn=`0x2DC984` can_call_parent=False is_parent_fn=False up=1
- access@`0x2FE1FE` fn=`0x2FE17E` can_call_parent=False is_parent_fn=False up=0
- access@`0x30D294` fn=`0x30D28C` can_call_parent=False is_parent_fn=False up=0
- access@`0x30FCDA` fn=`0x30FCAC` can_call_parent=False is_parent_fn=False up=1

## Ranked hypotheses (pre-live)

1. `MISSING_APP_INIT_DISPATCH` — hot clusters 0x2DFC3C/0x2E0E00/0x2DC778 never entered from boot
1. `MISSING_QUEUE_CONSUMER_TO_DISPATCHER` — 10165 writes FE8/B7D but consumer may never run or never call parent cluster
1. `MISSING_PLATFORM_SIDE_EFFECT_STATE_38` — state word still downstream of cold dispatcher path
1. `MISSING_RESOURCE_READY_DISPATCH` — only if live cluster hit + resource markers
1. `MISSING_NETWORK_READY_DISPATCH` — only if live cluster hit + network markers

## BP spec (role-tagged)

`p:0x300158,p:0x3002C0,p:0x300714,p:0x30103C,p:0x3020C8,p:0x302340,p:0x302362,e:0x2E0E00,u:0x3013A6,u:0x3013CA,u:0x3013B8,u:0x3013CE,e:0x2DFC3C,u:0x2DC620,u:0x2F508A,u:0x3005B2,u:0x30067A,u:0x30D5E0,u:0x2DC650,u:0x2F52DA,u:0x300606,u:0x30061C,u:0x3006EE,u:0x300704,e:0x2DC778,u:0x2DD09E,u:0x2DD3D0,e:0x2E7AC0,e:0x2F5436,e:0x2FA500,u:0x2DA4B0,u:0x2DC694,u:0x2DC716,u:0x2DC778,u:0x2DCC74,u:0x2DCE04,u:0x2DFCAC,u:0x2E0AD6,u:0x2DA4DC,u:0x2DA52C,u:0x2DC6D4,u:0x2DC718,u:0x2DC7BA,u:0x2DCC8A,e:0x302118,e:0x3...`

