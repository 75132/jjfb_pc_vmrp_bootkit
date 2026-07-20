#ifndef GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H
#define GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stage E8F/E8G/E8H: observe-only writer/caller/dispatcher BP + SVC trap.
 *
 * E8F Env:
 *   JJFB_E8F_WRITER_BP=1, JJFB_E8F_WRITER_PCS=...
 *   JJFB_E8F_SIBLING_PROBE=1, JJFB_E8F_SIBLING=...
 *   JJFB_E8F_COUNTERFACTUAL=C44|C9D|CF5|C44C9D|C44CF5|C9DCF5|ALL
 *   JJFB_E8F_LONGPATH_WATCH=1
 *
 * E8G Env:
 *   JJFB_E8G_CALLER_BP=1          — bootstrap caller CODE hooks
 *   JJFB_E8G_CALLER_PCS=0x..,..   — optional CSV (else built-in priority set)
 *   JJFB_E8G_FAULT_WATCH=1        — hook 0x2D92B0 + rich fault dump on lifecycle fail
 *
 * E8H Env:
 *   JJFB_E8H_DISPATCHER_BP=1      — 0x300714/0x30103C/0x3020C8/0x302340/... CODE hooks
 *   JJFB_E8H_DISPATCHER_PCS=...   — optional CSV override
 *   JJFB_E8H_SVC_TRAP=1           — observe-only trap at SVC #0xAB (0x2D92AE); no fake success
 *   JJFB_E8H_SVC_STOP=1           — uc_emu_stop on first SVC #0xAB (default when trap=1)
 *
 * E8I Env:
 *   JJFB_E8I_PARENT_BP=1          — 0x300158 + upstream BL sites + dispatcher chain
 *   JJFB_E8I_PARENT_PCS=0x..,..   — CSV (priority or all)
 *   JJFB_E8I_STATE_WATCH=1        — MEM_WRITE watch on R9+(0x800+0xD0); observe-only
 *
 * E8J Env:
 *   JJFB_E8J_CLUSTER_BP=1         — role-tagged cluster/upstream/queue CODE hooks
 *   JJFB_E8J_BP_SPEC=e:0x..,u:0x..,q:0x..,p:0x..,b:0x..
 *   JJFB_E8J_QUEUE_READ_WATCH=1   — MEM_READ watch on R9+FE8 / R9+B7D; observe-only
 *
 * E8K Env:
 *   JJFB_E8K_10102_CASE=<n>       — observe-only fire registered 0x10102 handler with R0=case
 *                                   (derived switch index; NOT product success)
 *
 * E8L Env:
 *   JJFB_E8L_10102_REGS=r0,r1,r2,r3 — structured ABI (overrides CASE when r0 present)
 *   JJFB_E8L_10102_R1=<n>         — R1 payload (event code or pointer); with E8K_CASE
 *   JJFB_E8L_10102_R2=<n>
 *   JJFB_E8L_10102_R3=<n>
 *
 * E8M Env:
 *   JJFB_E8M_PARENT_TRACE=1       — log first 200 insns in 0x300158..0x3004F6 (tick1)
 *   JJFB_E8M_SEQ=10165+310+156:18 — ordered observe-only fires (not product success)
 *
 * E8N Env:
 *   JJFB_E8N_CF_STATE=<n>         — COUNTERFACTUAL_ONLY poke R9+0x8D0 then case156
 *                                   (ladder map only; NOT product success)
 *
 * E8O-Fast Env (NOT product success):
 *   JJFB_FAST_ASSIST=1
 *   JJFB_FAST_STATE=20|38
 *   JJFB_FAST_CASE156_R1=18|20
 *   JJFB_FAST_SEQUENCE=case156|10165_case156|case310_case156|10165_case310_case156
 *   JJFB_FAST_SVC_AB=observe|return0|preserve
 *
 * E8P-Fast Env (NOT product success):
 *   JJFB_FAST_EEC7C=<n>           — poke *(R9+0xEEC+0x7C); early-arm field
 *   JJFB_FAST_DEC30=<n>           — poke *(R9+0xDEC+0x30); R1=20 success-arm gate
 *   JJFB_FAST_C6C22=<n>           — poke *(u8*)(R9+0xC6C+0x22); R1=20 CMP#1 gate
 *   JJFB_FAST_INSN_LIMIT=<n>      — raise case156 fire insn budget (default 400000)
 *
 * E8Q-Fast: R1=20 success arm + C44 nonzero unlock (0x2FC8CE); see RUN_E8Q_FAST.ps1
 *
 * E8U-DisplayFirst (NOT product success):
 *   JJFB_DISPLAY_FIRST=1           — arm idle-gate branch assist + draw watch
 *   JJFB_BYPASS_C9D_GATE=1         — at PC 0x3066C6 (C9D BNE), continue 0x3066C8 (no C9D poke)
 *   JJFB_BYPASS_CF5_GATE=1         — optional: after C9D assist, force success @ 0x306740
 *   JJFB_FAST_UI_UPSTREAM=2E2520   — call real event dispatcher with captured UI object
 *   JJFB_FAST_UI_OBJECT_R0=0x..    — explicit UI object for 0x2E4788 (never r0=0)
 *   JJFB_E8U_SCREENSHOT=path.bmp   — first nontrivial guiDrawBitmap → SDL_SaveBMP
 *
 * E8V-FirstFrame (NOT product success):
 *   JJFB_E8V_MODE=1                — deep-trace idle success 0x2E88CC toward real draw
 *   JJFB_E8V_E88CC_TRACE=1         — insn trace 0x2E88CC..0x2E8A4E (default on with MODE)
 *   JJFB_E8V_INSN_LIMIT=<n>        — max logged insns in 0x2E88CC (default 2000)
 *   JJFB_E8V_CALL_2E993C=1         — Case C: FAST call R9-only 0x2E993C after first idle OK
 *   Pair with DISPLAY_FIRST + C9D bypass (Case A base). No C9D/CF5 poke / no fake DRAW.
 *
 * E8W-FirstFrame (NOT product success):
 *   JJFB_E8W_MODE=1                — F6C embedded struct (F70/F74) acquire + re-enter 0x2E88CC
 *   JJFB_FAST_F6C_OBJECT_ASSIST=1  — map scratch table, set R9+F74 (structural only)
 *   JJFB_E8W_REENTER_E88CC=1       — re-enter 0x2E88CC after F70/F74 nonzero (default w/ assist)
 *   Gate: open if F74!=0 OR F70!=0 (not a heap object at [R9+F6C]).
 *   No C9D/CF5 poke / no fake DRAW / no framebuffer paint.
 *
 * E8Y-FirstFrame (NOT product success):
 *   JJFB_E8Y_MODE=1                — deep-trace 0x2D92E4 → A64 → 0x310BBC
 *   JJFB_E8Y_INSN_LIMIT=<n>        — max logged insns in 0x2D92E4 (default 5000)
 *   JJFB_FAST_A64_RESOURCE_ASSIST=1 — structural A58/A5C/A60/A64 handles (no pixels)
 *   Implies E8X Case C baseline (F74 + dims). Resource name: wy_jiao*!w!h.bmp
 *
 * E8Z-FirstPixel (NOT product success):
 *   JJFB_E8Z_MODE=1                — real BMP member → nonzero bmp → screenshot
 *   JJFB_FAST_REAL_BMP_HANDLE=1    — handle+4 = real RGB565 from jjfb.mrp member
 *   JJFB_DISPLAY_FIRST_MEMBER_FASTPATH=1 — skip stalled 0x304BF0; return real handle
 *   JJFB_E8Z_BMP_PATH=<file>       — pre-extracted wy_jiao1!11!11.bmp (raw RGB565)
 *   Pixels must come from original jjfb.mrp; no invented bitmap data.
 *
 * E9A-FirstFrame (NOT product success):
 *   JJFB_E9A_MODE=1                — stabilize first frame + naturalize member resolve
 *   JJFB_REAL_MRP_MEMBER_BRIDGE=1  — at 0x304BF0, decode exact jjfb.mrp members (real bytes)
 *   JJFB_REAL_MRP_PATH=<jjfb.mrp>  — original package path for bridge decode
 *   Prefer bridge over FAST_REAL_BMP_HANDLE when possible.
 *
 * E9G-Splash/Loading UI (NOT product success):
 *   JJFB_E9G_MODE=1                — splash UI_MODE=0x45 + real 0x2EF86C path
 *   JJFB_FAST_SPLASH_CALL=1        — FAST call splash fn 0x2EF86C (r0=0x45,r1=0x13)
 *   JJFB_E9G_UI_MODE_ASSIST=1      — optional poke R9+0x8D0=0x45 before splash (not product)
 *   JJFB_E9G_DEBUG=1               — trace-only; no splash guest call
 *   JJFB_E9G_REQUEST_CSV=<path>    — request/ui_mode/postmatch rows
 *   JJFB_E9G_UIMODE_CSV=<path>     — UI_MODE write/assist/splash rows
 *   Implies E9F/E9E/E9D natural postmatch. Does NOT enable JJFB_E9F_REWRITE_REQUEST
 *   (rewrite under E9G is debug-only demotion, not success).
 *
 * E9H-Splash blit / r4 (NOT product success):
 *   JJFB_E9H_MODE=1                — reach 0x2EFA9A → 0x2EC6B8 loadingbar blit
 *   JJFB_E9H_R4_TRACE=1            — log r4 changes in 0x2EF86C..0x2EFB20
 *   JJFB_FAST_SPLASH_R4_ASSIST=1   — optional r4 object assist after layout known
 *   JJFB_E9H_R4_CSV=<path>         — r4 provenance rows
 *   JJFB_E9H_SEQ_CSV=<path>        — splash resource sequence
 *   robotol code_base refined to 0x2D8DF4: blit entry 0x2EC6B8, call 0x2EFA9A.
 *   After loadingbar postmatch, skip at 0x2EFA46 → 0x2EFA5C (y-calc then blit).
 *   Never jump from 2D92E4 / never skip to 0x2EFA9E (that's post-blit).
 *   No request rewrite. No blind jump to 0x2F45A2.
 *
 * E9I-Splash Loading UI (NOT product success):
 *   JJFB_E9I_MODE=1                — complete splash UI; seed R9+0x830/0x834
 *   JJFB_SPLASH_COORD_ASSIST=1     — diagnostic xy recovery only (not success)
 *   JJFB_E9I_SKIP_SIBLING=1        — optional E9H-style skip bar/textbar binds
 *   JJFB_E9I_COORD_CSV / R4_CSV / SEQ_CSV
 *   Natural xy: x=(R9_830-bmp_w)/2 → R6; y=(R9_834)-100 → R5. Default keeps siblings.
 *
 * E9J-Splash progress / post-r4 (NOT product success):
 *   JJFB_E9J_MODE=1                — R9+0xBD0 status string + post-r4 path
 *   JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST=1 — seed BD0=robotol "请稍候" + BA0+0x2C count
 *   JJFB_E9J_PROGRESS_COUNT=<1..12>
 *   JJFB_E9J_WRITER_CSV / JJFB_E9J_POSTR4_CSV
 *   Natural writer: 0x2FC418 → 2d9648 concat → STR @ 0x2FC444 to BA0+0x30.
 *   Gate: B6C!=0 skips r4; else r4=*(BD0) must be nonzero for text path.
 *
 * E9K-Post-r4 textbar / 0x305BFC (NOT product success):
 *   JJFB_E9K_MODE=1                — delay hold; trace 0x2EFAFA→0x2EFB0E→0x305BFC
 *   JJFB_E9K_HOLD_AFTER_POST_R4=1  — do not stop on early progress blits
 *   JJFB_E9K_MIN_BLITS=<n>         — blit-count fallback floor (default high)
 *   JJFB_E9K_STOP_ON_TEXTBAR_DRAW=1
 *   JJFB_E9K_POSTR4_CSV / JJFB_E9K_DRAW_CSV
 *   Keep BD0 assist; natural BD0 writer is deferred. Hold via jjfb_e9k_request_hold.
 *
 * E9L-Text context 818/81C (NOT product success):
 *   JJFB_E9L_MODE=1                — R9+0x818/0x81C dims for 0x305E78→0x305BFC
 *   JJFB_FAST_TEXTCTX_ASSIST=1     — seed 818=240,81C=320 (diagnostic only)
 *   JJFB_E9L_WRITER_CSV / 305E78_CSV / TEXT_CSV
 *   818/81C are screen dims (same family as 830/834), not a font object pointer.
 *
 * E9R-Platform screen dims (NOT product success):
 *   JJFB_E9R_MODE=1                — naturalize TEXTCTX via platform dims
 *   JJFB_PLATFORM_SCREEN_DIMS=1    — fill zero 818/81C/830/834/824 from host surface
 *   Success path: PLATFORM_SCREEN_DIMS=1, FAST_TEXTCTX_ASSIST off.
 *   Prefer already-seeded 830/834 when sane; else 240x320 host framebuffer.
 *
 * E9S-BD0 naturalization (NOT product success):
 *   JJFB_E9S_MODE=1                — trace 0x2FC418/0x2FC444 BD0 writers
 *   JJFB_FAST_BD0_INIT_CALL=1      — call real 0x2FC418 (r0=guest C-string)
 *   JJFB_E9S_BD0_STR_VA=<va>       — optional; default 0x3146C4 (2FC03C natural arg)
 *   JJFB_E9S_WRITER_CSV / FN_CSV
 *   Success path: FAST_BD0_INIT_CALL on, FAST_SPLASH_PROGRESS_OBJECT_ASSIST off.
 *   0x2FC418: concat via 2D9648 → STR @0x2FC444 to BA0+0x30 (=R9+BD0); ui_mode=0x45.
 *
 * E9M-Text ABI / measure return (NOT product success):
 *   JJFB_E9M_MODE=1                — decode 0x305BFC ABI; fix measure/layout r1
 *   JJFB_FAST_TEXT_MEASURE_SHIM=1  — write plausible w/h after plat 0x12340
 *   JJFB_FAST_TEXT_LAYOUT_ASSIST=1 — repair bad x at real caller 0x2EFBA2
 *   JJFB_E9M_ABI_CSV / MEAS_CSV / LAYOUT_CSV
 *   Root cause: x=(W-height_out)/2; garbage height → r1=0xFFE7917B.
 *
 * E9N-305C3C glyph / platform text draw (NOT product success):
 *   JJFB_E9N_MODE=1                — deep trace 0x305C3C inner clip/glyph/blit
 *   JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM=1 — mr_platDrawChar/_DrawText compat only
 *   JJFB_E9N_GLYPHTRACE=1          — focus font/glyph table reads
 *   JJFB_E9N_305C3C_CSV / CLIP_CSV / GLYPH_CSV
 *   Inner gates: R9+830/824 clip, 303C50 draw mode, BL 2F2360 glyph blit.
 *
 * Never claims counterfactual as product success.
 * Never returns blind success from SVC #0xAB.
 * Never force-writes state word or idle flags as product success.
 * DISPLAY_FIRST must not poke C9D/CF5 memory or paint framebuffer from host.
 */

int robotol_flag_writer_trace_enabled(void);
void robotol_flag_writer_trace_reset(void);
void robotol_flag_writer_trace_bind_uc(void *uc);
void robotol_flag_writer_trace_set_tick(uint32_t tick);

void robotol_flag_writer_trace_try_arm(void *uc);
void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick);
void robotol_flag_writer_trace_dump_summary(const char *reason);

/* E8G: after 10140 fire fails — dump regs/context at fault PC (COUNTERFACTUAL path). */
void robotol_flag_writer_trace_on_lifecycle_fault(void *uc, uint32_t tick, int ok,
                                                  unsigned uc_err, uint32_t pc_after,
                                                  uint32_t r0_after, uint32_t r9_after,
                                                  uint32_t sp_after, uint32_t lr_after);

/* E9N: platform 0x11F00 drawText compat (JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM).
 * Draws real guest splash string at real param0 x/y. NOT product success. */
int jjfb_e9n_try_plat_11f00_text_draw(void *uc, uint32_t app, uint32_t code_obj,
                                      uint32_t param0);

#ifdef __cplusplus
}
#endif

#endif
