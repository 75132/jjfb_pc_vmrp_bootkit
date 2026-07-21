#ifndef GWY_LAUNCHER_EXT_CHUNK_PROVIDER_H
#define GWY_LAUNCHER_EXT_CHUNK_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6N: platform-owned mrc_extChunk publication (gated).
 * Env: JJFB_EXTCHUNK_PROVIDER=off|gbrwcore_only|gbrwcore_wxjwq|gwy_shell|shell_core|
 *      shell_and_game
 *      (shell_core: gbrwcore|gamelist|gbrwshell)
 *      (shell_and_game: shell_core + mrc_loader|robotol|mmochat)
 *      JJFB_EXTCHUNK_SLOT_TRACE=1
 * Never invents game-specific P+0xC targets; uses DOCUMENTED mrc_extChunk_st layout.
 */

#define GWY_MRC_EXTCHUNK_CHECK 0x7FD854EBu
#define GWY_MRC_EXTCHUNK_BYTES 0x40u

typedef struct GwyMrcExtChunk {
    uint32_t check;         /* +0x00 DOCUMENTED 0x7FD854EB */
    uint32_t init_func;     /* +0x04 mr_c_function_load / image+8 */
    uint32_t event;         /* +0x08 mr_helper */
    uint32_t code_buf;      /* +0x0C */
    uint32_t code_len;      /* +0x10 */
    uint32_t var_buf;       /* +0x14 */
    uint32_t var_len;       /* +0x18 */
    uint32_t global_p_buf;  /* +0x1C mr_c_function_st* */
    uint32_t global_p_len;  /* +0x20 */
    uint32_t timer;         /* +0x24 */
    uint32_t send_app_event;/* +0x28 mrc_extMainSendAppMsg_t */
    uint32_t ext_mr_table;  /* +0x2C */
    uint32_t pad[4];
} GwyMrcExtChunk;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(offsetof(GwyMrcExtChunk, check) == 0x00, "check");
_Static_assert(offsetof(GwyMrcExtChunk, init_func) == 0x04, "init_func");
_Static_assert(offsetof(GwyMrcExtChunk, event) == 0x08, "event");
_Static_assert(offsetof(GwyMrcExtChunk, send_app_event) == 0x28, "sendAppEvent");
_Static_assert(sizeof(GwyMrcExtChunk) >= GWY_MRC_EXTCHUNK_BYTES, "size");
#endif

typedef enum GwyExtChunkProviderMode {
    GWY_EXTCHUNK_OFF = 0,
    GWY_EXTCHUNK_GBRWCORE_ONLY = 1,
    GWY_EXTCHUNK_GBRWCORE_WXJWQ = 2,
    GWY_EXTCHUNK_GWY_SHELL = 3,
    GWY_EXTCHUNK_SHELL_AND_GAME = 4,
    /* Product track: mrc_loader / robotol / mmochat only (no shell). */
    GWY_EXTCHUNK_GAME_PACKAGE = 5
} GwyExtChunkProviderMode;

void ext_chunk_provider_reset(void);
void ext_chunk_provider_bind_uc(void *uc);
void ext_chunk_provider_set_sendappevent_guest(uint32_t guest_addr);
void ext_chunk_provider_set_mr_table_guest(uint32_t guest_addr);
uint32_t ext_chunk_provider_mr_table_guest(void);

int ext_chunk_provider_enabled(void);
GwyExtChunkProviderMode ext_chunk_provider_mode(void);
const char *ext_chunk_provider_mode_name(GwyExtChunkProviderMode m);

/* Bridge: whether to alloc a platform extChunk for this helper (gated). */
int ext_chunk_provider_want(uint32_t helper);

/* If a chunk already exists for helper/P, refill+publish and return 1 (no new alloc). */
int ext_chunk_provider_try_reuse(void *uc, uint32_t helper, uint32_t p_guest, void *p_host);

/* Bridge: after P alloc. p_host is host pointer to mr_c_function_st; chunk_* from my_mallocExt. */
int ext_chunk_provider_on_c_function_new(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                                         void *chunk_host, uint32_t chunk_guest);

/* After documented entry emu may zero-init P — restore platform publication if cleared. */
void ext_chunk_provider_after_entry_order(uint32_t helper);

/* After EXT_REGISTER or when P+0xC was cleared by guest zero-init. */
void ext_chunk_provider_on_module_registered(const char *module_name, uint64_t module_id,
                                             uint32_t helper, uint32_t code_base, uint32_t code_size,
                                             uint32_t p_guest, void *p_host);
void ext_chunk_provider_on_pxc_cleared(uint32_t p_guest, uint32_t pc);

void ext_chunk_provider_on_slot28_call(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2,
                                       uint32_t r3, uint32_t r4, uint32_t ret);

void ext_chunk_provider_finalize(const char *stop_reason);

int ext_chunk_provider_published(void);
uint32_t ext_chunk_provider_last_chunk_guest(void);
uint32_t ext_chunk_provider_last_p_guest(void);
const char *ext_chunk_provider_last_module(void);
/* 1 if guest addr is a published mrc_extChunk; optionally returns host pointer. */
int ext_chunk_provider_lookup_chunk(uint32_t chunk_guest, void **out_chunk_host);
/* DOCUMENTED mrc_extChunk.timer at +0x24. */
int ext_chunk_provider_set_timer_field(uint32_t chunk_guest, uint32_t timer_id);
/* DOCUMENTED: EXT timer fire → helper(P, code=2) → mrc_timerTimeout. */
int ext_chunk_provider_timer_dispatch_target(uint32_t chunk_guest, uint32_t *out_helper,
                                            uint32_t *out_p_guest, uint32_t *out_erw);

/* Phase 6O: look up platform-owned P/chunk for nested module. */
int ext_chunk_provider_lookup_p(uint32_t p_guest, void **out_p_host, uint32_t *out_helper,
                                uint32_t *out_chunk_guest);
/* Mirror DOCUMENTED mrc_extChunk var_buf/var_len from P+0/+4. */
int ext_chunk_provider_set_var_fields(uint32_t p_guest, uint32_t var_buf, uint32_t var_len);
/* Peek P+0 / P+4 via host pointer when known. Returns 1 if p_host found. */
int ext_chunk_provider_peek_p_er_rw(uint32_t p_guest, uint32_t *out_base, uint32_t *out_len);

typedef struct ExtChunkOwnerInfo {
    uint32_t helper;
    uint32_t p_guest;
    uint32_t chunk_guest;
    uint32_t erw;
    uint32_t p_erw;
    uint32_t chunk_erw;
    uint32_t registry_erw;
    uint64_t module_id;
    uint32_t module_generation;
    uint32_t p_generation;
    char module_name[48];
    char package_name[128];
} ExtChunkOwnerInfo;

int ext_chunk_provider_owner_for_chunk(uint32_t chunk_guest, ExtChunkOwnerInfo *out);
int ext_chunk_provider_owner_for_helper(uint32_t helper, ExtChunkOwnerInfo *out);
int ext_chunk_provider_owner_for_p(uint32_t p_guest, ExtChunkOwnerInfo *out);
int ext_chunk_provider_owner_for_erw(uint32_t erw, ExtChunkOwnerInfo *out);
int ext_chunk_provider_rebind_chunk_erw(uint32_t chunk_guest, uint32_t new_erw, uint32_t p_guest);

#ifdef __cplusplus
}
#endif

#endif
