# P22F-CLEAN +0x10740 scheduler provenance verdict

## Bottom line

**Class: C**

callback itself never registered — registration-producing Guest init not observed; +0xF670/+0x8CDC/+0xD978 never entered

## Identity

```
source commit: 3a1fdfa12492827ce1a183af464001d7c6398841
main.exe SHA: b108a66114e2beec2609457edb7bcffb44b941a330c2798ebe41375a9249420b
raw gamelist.ext SHA: 70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime image SHA: e510fe8795381f27e1ec49048f04ee94a486baf0ede0c9382825d2d606427ca8
runtime base/end: 0x2D4364 / 0x2EA940
runtime size: 0x165DC
raw_base_refine_pad: 0x4
module id: 0x4
ERW: 0x682B8C
P: 0x2AC8EC
generation: 2
package owner: gamelist.ext
identity_gaps: none
```

## Runtime callers

```
xref_n=11 static11_verified=yes extra_direct=0 literal_ptrs=0
```

### A group (+0x392C)

```
+0xF670 hit=0 lr=0x0
+0x392C hit=0 r0=0x0 r1=0x0 r2=0x0 r3=0x0 r9=0x0
  callsite +0x4076 hit=0
  callsite +0x4526 hit=0
  callsite +0x458A hit=0
  callsite +0x46C4 hit=0
  callsite +0x4778 hit=0
  callsite +0x4C76 hit=0
  callsite +0x53A4 hit=0
  callsite +0x5904 hit=0
  callsite +0x5918 hit=0
```

### B group (+0x8CDC)

```
+0x8CDC hit=0
+0x8D26 hit=0
registrations=0
```

### C group (+0xD978)

```
+0xD978 hit=0
+0x12CF0 hit=0
+0x12D0E hit=0
[R9+0x450] snap=0x0 (prep only if 10740 not entered)
```

## PASS answers

```
nearest +0x10740 caller path: NONE of A/B/C entered
first blocking branch: n/a
comparison operands: n/a
actual path: NEVER_REGISTERED / never entered
target path: register+deliver F670/8CDC/D978 producer
natural producer: unknown
+0x10740 natural enter: 0
[R9+0x3E4]: 0x0
[R9+0x6C4]: 0x1
+0x10814: 0
+0xFF00: 0
+0x7B6C: 0
Guest state written: NO
events injected: NO
headless: NO
current sole lock: callback itself never registered — registration-producing Guest init not observed; +0xF670/+0x8CDC/+0xD978 never entered
next minimal fix: restore natural producer / platform delivery contract (observe-only this round)
stop_reason: timer_fire_n12
fire_ext_n: 12
gl_insn_n: 8894
```
