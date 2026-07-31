# P14 — `mr_getUserInfo` verdict

## Summary

`mr_getUserInfo` is a real product MAP_FUNC (`bridge` @ `0x8C` / slot `0x280090`): fills exactly **64** bytes of DOCUMENTED `mr_userinfo` via shared `platform_userinfo_current()`, returns `MR_SUCCESS`/`MR_FAILED`. Natural post-Case-9 path calls it with **`info=NULL`** (stale event regs `R0=0 R1=0x9`) → **`MR_FAILED`**, matching mythroad `dsm.c`. That clears the P13 exit door; chase wired **`mr_sleep`** (`0x90`) with absurd-ms clamp. New hard stop: **`mr_plat` @ `0x280098`**.

Frozen ABI unchanged: `direct_lr`, `product_ffp_apply_abi=0`, `JJFB_PRODUCT_EVENT_CONTRACT=0`, `JJFB_FAMILY_4F_FOR_E6C=0`.

## Implementation

| Piece | Detail |
|-------|--------|
| Shared identity | `platform_userinfo_current()` → defaults + `GWY_DEFAULT_IMSI` |
| `0x10180` | `platform_send_app_event_classify` uses same helper (pointer return unchanged) |
| Bridge | `br_mr_getUserInfo`: null/unwritable → `MR_FAILED`; else poke 64 bytes → `MR_SUCCESS` |
| Logging | first 16 calls: owner/package/`current_mrp`; leave note + optional sha256 |
| Chase | `mr_sleep` → existing `br_sleep`; `ms>10000` clamped to 0 (post-fail `R0=-1`) |
| Tests | spare zeros, sentinel beyond +0x40, current≡fill≡0x10180 first 64 |

## Natural-path evidence (hit samples)

Binary: `main_exe_sha256=63321b9648027d9f5c4619d25a743146277af6bcc6e734331f511a82689c3615`  
Matrix: `reports/p14_userinfo_run_matrix.csv`

| run | applicable | contract | owner | `current_mrp` | sleep | next door | dsm_reinit |
|-----|------------|----------|-------|---------------|-------|-----------|------------|
| diag | HIT | null_ptr → −1 | robotol.ext | gwy/jjfb.mrp | yes | mr_plat | 1 |
| hit1 | HIT | null_ptr → −1 | robotol.ext | gwy/jjfb.mrp | yes | mr_plat | 1 |
| extra2 | HIT | null_ptr → −1 | robotol.ext | gwy/jjfb.mrp | yes | mr_plat | 1 |

Other attempts: `NOT_APPLICABLE` (Case-9 leave without reaching getUserInfo — timing/scheduling flake, not API failure).

Observed chain after Case-9 leave:

```text
getCharBitmap → getTime → getUserInfo(NULL)→MR_FAILED → mr_sleep(clamped) → mr_plat UNIMPLEMENTED → exit
```

No `MR_GETUSERINFO_PACKAGE_ALERT`; `current_mrp` stayed `gwy/jjfb.mrp` on hits (jjfb ownership). No natural non-NULL write on this short path (`leave_write=0`); unit tests cover 64-byte layout + sentinel.

## Checklist

```text
mr_getUserInfo 是否真实实现：是（platform_userinfo_current + Guest poke 64 + MAP_FUNC）
实际 ABI：R0=mr_userinfo*；自然首呼 R0=NULL → MR_FAILED（与 dsm.c 一致）
与 0x10180 是否同数据源：是（不同返回契约：写结构 vs 返回 blob 指针）
mmochat / Case-9 是否越过原退出点：是（不再 Not yet implemented getUserInfo）
是否仍发生 DSM reinit：是（次数≈1；现因 mr_plat exit，非 getUserInfo）
是否出现真实画面：否
旧五 BMP / Layer-1：未判定（路径未到）
getUserInfo 后第一个自然行为：mr_sleep（已 chase）→ mr_plat（未实现）
当前下一个唯一门锁：mr_plat @ 0x280098（MAP_FUNC NULL）
停滞类型：A（缺标准平台服务）
能否继续向登录界面推进：是（下一轮优先 mr_plat，勿重开 Case-9 / Host UI）
```

## Next minimal fix (P15)

```text
API: mr_plat
slot: 0x280098  offset: 0x94
decl: int32 mr_plat(int32 code, int32 param);
current handler: NULL
note: only implement when natural hit supplies real (code,param); do not guess mega-switch
```
