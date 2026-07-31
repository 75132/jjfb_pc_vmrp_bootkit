# Phase 6M — P struct timeline

| seq | excerpt |
|---:|---|
| 1 | `seq=1 pc=0x0 module=host:_mr_c_function_new op=alloc_memset P=0x2803E4 off=0x0 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=DOCUMENTED` |
| 2 | `seq=2 pc=0x8003C module=unknown op=P_WRITE P=0x2803E4 off=0x08 old=0x0 new=0x1 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 3 | `seq=3 pc=0x8008C module=unknown op=P_WRITE P=0x2803E4 off=0x04 old=0x0 new=0x25D0 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 4 | `seq=4 pc=0x80098 module=unknown op=P_WRITE P=0x2803E4 off=0x00 old=0x0 new=0x280400 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 5 | `seq=5 pc=0x0 module=host:_mr_c_function_new op=alloc_memset P=0x2AC8DC off=0x0 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=DOCUMENTED` |
| 6 | `seq=6 pc=0xA05A4 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x29EA0 lr=0x2AC3A4 class=metadata evidence=TARGET_OBSERVED` |
| 7 | `seq=7 pc=0xA05A4 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x29EA0 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 8 | `seq=8 pc=0xA05A4 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0x18 lr=0x2AC3A4 class=metadata evidence=TARGET_OBSERVED` |
| 9 | `seq=9 pc=0xA05A4 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0x18 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 10 | `seq=10 pc=0xA05C0 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x29EA0 new=0x2AF58 lr=0x2AC3A4 class=metadata evidence=TARGET_OBSERVED` |
| 11 | `seq=11 pc=0xA05C0 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x2AF58 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 12 | `seq=12 pc=0xA05D0 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x18 new=0xA00 lr=0x2AC3A4 class=metadata evidence=TARGET_OBSERVED` |
| 13 | `seq=13 pc=0xA05D0 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0xA00 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 14 | `seq=14 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x2AF58 new=0x0 lr=0x89D5C class=zero_init evidence=TARGET_OBSERVED` |
| 15 | `seq=15 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=TARGET_OBSERVED` |
| 16 | `seq=16 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0xA00 new=0x0 lr=0x89D5C class=zero_init evidence=TARGET_OBSERVED` |
| 17 | `seq=17 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=TARGET_OBSERVED` |
| 18 | `seq=18 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x08 old=0x0 new=0x0 lr=0x89D5C class=zero_init evidence=TARGET_OBSERVED` |
| 19 | `seq=19 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x08 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=TARGET_OBSERVED` |
| 20 | `seq=20 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x0C old=0x0 new=0x0 lr=0x89D5C class=zero_init evidence=TARGET_OBSERVED` |
| 21 | `seq=21 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x0C old=0x0 new=0x0 lr=0x0 class=zero_init evidence=TARGET_OBSERVED` |
| 22 | `seq=22 pc=0x94F04 module=dsm:cfunction.ext op=P_WRITE P=0x2AC8DC off=0x10 old=0x0 new=0x0 lr=0x89D5C class=zero_init evidence=TARGET_OBSERVED` |
| 23 | `seq=23 pc=0x0 module=host:_mr_c_function_new op=alloc_memset P=0x2AC8DC off=0x0 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=DOCUMENTED` |
| 24 | `seq=24 pc=0x30CAA6 module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x08 old=0x0 new=0x1 lr=0xA2548 class=metadata evidence=TARGET_OBSERVED` |
| 25 | `seq=25 pc=0x30CAA6 module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x08 old=0x0 new=0x1 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 26 | `seq=26 pc=0x30CAC0 module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0x19A8 lr=0x30CABF class=metadata evidence=TARGET_OBSERVED` |
| 27 | `seq=27 pc=0x30CAC0 module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x04 old=0x0 new=0x19A8 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 28 | `seq=28 pc=0x30CACE module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x2B0D14 lr=0x30CACB class=metadata evidence=TARGET_OBSERVED` |
| 29 | `seq=29 pc=0x30CACE module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x2B0D14 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |
| 30 | `seq=30 pc=0x30CADE module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x2B0D14 new=0x2B0D18 lr=0x30CACB class=metadata evidence=TARGET_OBSERVED` |
| 31 | `seq=31 pc=0x30CADE module=gbrwcore.ext op=P_WRITE P=0x2AC8DC off=0x00 old=0x0 new=0x2B0D18 lr=0x0 class=metadata evidence=TARGET_OBSERVED` |

- timeline events: `31`
- 0x94F04 zero writer: `yes`
- CFN_PXC_SOURCE: `yes`

