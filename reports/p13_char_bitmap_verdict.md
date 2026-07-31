# P13-FAST Verdict — `mr_getCharBitmap` + next natural door

## Bottom line

`mr_getCharBitmap` is a real product MAP_FUNC (sky16/`gb16.uc2`, Guest VA return, MSB-first 1=ink). Clean frozen runs show Case-9 leave → `mmochat.ext` → `getCharBitmap` returns nonzero guest VA → `mr_getTime` succeeds → hard stop on **`mr_getUserInfo` @ `0x280090`** (`NULL` handler → `exit`/DSM reinit). Stagnation type remains **A** (missing standard platform service), one step past getCharBitmap.

## Bit-order evidence (not guessed)

| Source | Finding |
|--------|---------|
| `mythroad/include/mrporting.h` | each bit = one pixel; rows byte-aligned; width 12 → 2 bytes/row, low bits zero |
| `mythroad/dsm.c` `xl_font_sky16_drawChar` | `data & (1<<15)` then `<<=1` → **MSB-first**, **1=ink** |
| `mythroad/mythroad.c` `MR_FONT_LIB_REDUNDANCY_BIT` path | `bitmap[b_] & (0x80 >> a_)` → same MSB-first polarity |
| Emitted layout | sky16 16×{8,16}: always 32-byte record, 2 bytes/physical row |

## ABI (runtime, first 32 calls)

Formal AAPCS holds:

```text
R0 = ch
R1 = fontSize
R2 = width_ptr (may be 0)
R3 = height_ptr (may be 0)
R0_out = guest glyph VA
```

First natural call observed:

```text
ch=0x0  fontSize=0x9  width_ptr=0  height_ptr=0  lr=0x304599
→ guest=0x2829E4  w=8 h=16 row_bytes=1  source=gb16.uc2
```

No declaration conflict (NULL outs allowed; `fontSize` is `uint16`, not restricted to `MR_FONT_*` enum). CJK natural requests were **not** seen on this short post-Case-9 path before `getUserInfo` abort; unit tests confirm U+8F7D etc. have real ink from `gb16.uc2`.

## Implementation

| Piece | Path |
|-------|------|
| Shared font API | `src/platform/platform_char_bitmap.c` + `.h` |
| Bridge | `br_mr_getCharBitmap` → `BRIDGE_FUNC_MAP(0x78, …)` |
| Guest buffer | `gwy_ext_obs_guest_malloc0_ex` + host fill + bounded cache (48) |
| VM reset | `platform_char_bitmap_reset` from obs reset; getTime base reset on `start_dsm` |
| PBM dump | `out/p13/glyph_U0000_font9.pbm` (env `JJFB_P13_GLYPH_DUMP=1`) |
| Unit test | `tests/unit/test_platform_char_bitmap.c` |

### Follow-on in-budget (one simple API)

After getCharBitmap cleared, next door was `mr_getTime` (`0x84` / slot `0x280088`). Implemented `br_mr_getTime` (uptime ms) and wired existing `br_getDatetime` to `mr_getDatetime`. **Did not** implement `mr_getUserInfo` (next unique door).

## Run matrix (frozen `main.exe` SHA `fea9654e…864de`)

See `reports/p13_char_bitmap_run_matrix.csv`.

| kind | Case-9 leave | mmochat | unimpl getChar | next unimpl | notes |
|------|--------------|---------|----------------|-------------|-------|
| diag/golden/visual | yes | yes | **no** | `mr_getUserInfo` (when markers present) | getTime no longer exits |
| strong product gates | yes ×3 | — | — | — | `RUN_PRODUCT_DIRECT_JJFB` exit 0 |

DSM `initMemoryManager` still repeats because `mr_getUserInfo` still `exit(1)`.

## Visual / five-BMP regression

- No `JJFB_BMP_REQ` in these windows → path still does **not** traverse old Layer-1 five-BMP splash after Case-9.
- Not classified as “five-BMP capability broken”; same entry now dies on `getUserInfo` before splash/text continuation.
- `DispUp`/window refresh markers appear; no verified Layer-1 SHA / real login frame.

## Required answers

```text
mr_getCharBitmap 是否真实实现：是（sky16/gb16.uc2 + Guest VA + bridge MAP_FUNC）
实际 ABI：R0=ch R1=fontSize R2=width* R3=height*；首呼 ch=0 font=9 outs=NULL
返回是否为 Guest VA：是（例 0x2829E4，MRP heap）
自然请求的第一个字符：U+0000（控制/空探测；非 CJK）
中文字符是否有真实点阵：单测是（U+8F7D ink）；自然路径本窗未请求到 CJK
宽高与行对齐是否正确：是（8×16 rb=1 / 16×16 rb=2；padding LSB 清零）
mmochat.ext 是否越过原退出点：是（不再 Not yet implemented mr_getCharBitmap）
是否仍发生 DSM reinit：是（现因 mr_getUserInfo exit）
是否出现真实画面：否（无 Layer-1 / 无五 BMP / 无稳定登录帧）
旧五 BMP / Layer-1 是否回归通过：未判定（路径未经过）；能力未证明损坏
getCharBitmap 后第一个自然行为：mr_getTime（已实现）→ mr_getUserInfo（未实现）
当前下一个唯一门锁：mr_getUserInfo @ 0x280090（MAP_FUNC NULL）
能否继续向登录界面推进：是（继续补标准平台服务；下一轮优先 getUserInfo，可复用 platform_userinfo）
```

## Next minimal fix (P14)

```text
API: mr_getUserInfo
slot: 0x280090  offset: 0x8C
decl: int32 mr_getUserInfo(mr_userinfo *info);
current handler: NULL
host asset: src/platform/platform_userinfo.c + platform_identity (already golden-tested)
```
