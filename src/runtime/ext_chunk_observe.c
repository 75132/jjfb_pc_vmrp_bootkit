#include "gwy_launcher/ext_chunk_observe.h"
#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/ext_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct {
    ExtChunkRecord records[GWY_CHUNK_OBS_MAX];
    size_t count;
    uint64_t next_id;
    void *uc;
    int any_scalar_write;
    int any_block_copy;
    int case_emitted;
} ExtChunkObserveState;

static ExtChunkObserveState g_chunk;

void ext_chunk_observe_reset(void) {
#ifdef GWY_HAVE_UNICORN
    size_t i;
    if (g_chunk.uc) {
        for (i = 0; i < g_chunk.count; i++) {
            if (g_chunk.records[i].watch_armed && g_chunk.records[i].write_hook) {
                uc_hook_del((uc_engine *)g_chunk.uc, (uc_hook)g_chunk.records[i].write_hook);
            }
        }
    }
#endif
    memset(&g_chunk, 0, sizeof(g_chunk));
}

void ext_chunk_observe_bind_uc(void *uc) {
    g_chunk.uc = uc;
}

int ext_chunk_provenance_enabled(void) {
    const char *env = getenv("GWY_CHUNK_PROVENANCE");
    return env && env[0] == '1';
}

const char *gwy_chunk_origin_name(GwyChunkOrigin o) {
    switch (o) {
        case GWY_CHUNK_ORIGIN_DSM_ALLOCATED: return "DSM_ALLOCATED";
        case GWY_CHUNK_ORIGIN_GUEST_ALLOCATED: return "GUEST_ALLOCATED";
        case GWY_CHUNK_ORIGIN_COPIED_FROM_TEMPLATE: return "COPIED_FROM_TEMPLATE";
        case GWY_CHUNK_ORIGIN_LOADER_RETURNED: return "LOADER_RETURNED";
        case GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C: return "FROM_P_PLUS_0C";
        default: return "UNKNOWN";
    }
}

const char *gwy_chunk_state_name(GwyChunkState s) {
    switch (s) {
        case GWY_CHUNK_STATE_FIRST_SEEN: return "FIRST_SEEN";
        case GWY_CHUNK_STATE_BEFORE_SELECT: return "BEFORE_SELECT";
        case GWY_CHUNK_STATE_AFTER_SELECT: return "AFTER_SELECT";
        default: return "UNKNOWN";
    }
}

const char *ext_chunk_field_04_class(uint32_t raw) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t n;
    if (!raw) return "unknown";
    n = raw & ~1u;
    if (n < 0x10000u) return "data";
    reg = gwy_ext_loader_bound_registry();
    if (reg) {
        m = module_registry_find_by_code_addr(reg, n);
        if (m) {
            if (raw & 1u) return "abs_thumb";
            return "abs_arm";
        }
    }
    /* Secondary pointer heuristic: aligned lowish RW-looking VA without code hit. */
    if ((raw & 3u) == 0 && n >= 0x10000u && n < 0x400000u) return "secondary_ptr";
    return "unknown";
}

ExtChunkRecord *ext_chunk_observe_find(uint32_t guest_base) {
    size_t i;
    if (!guest_base) return NULL;
    for (i = 0; i < g_chunk.count; i++) {
        if (g_chunk.records[i].guest_base == guest_base) return &g_chunk.records[i];
    }
    return NULL;
}

ExtChunkRecord *ext_chunk_observe_find_covering(uint32_t addr) {
    size_t i;
    if (!addr) return NULL;
    for (i = 0; i < g_chunk.count; i++) {
        ExtChunkRecord *r = &g_chunk.records[i];
        if (r->guest_base && addr >= r->guest_base && addr < r->guest_base + r->size)
            return r;
    }
    return NULL;
}

static int popcount_u32(unsigned v) {
    int n = 0;
    while (v) {
        n += (int)(v & 1u);
        v >>= 1;
    }
    return n;
}

void ext_chunk_observe_add_evidence(ExtChunkRecord *rec, unsigned evidence_bit) {
    int bits;
    if (!rec || !evidence_bit) return;
    rec->identity_evidence |= evidence_bit;
    bits = popcount_u32(rec->identity_evidence);
    if (bits >= 3)
        rec->identity_confidence = (int)EXT_IDENTITY_CONFIRMED;
    else if (bits >= 2)
        rec->identity_confidence = (int)EXT_IDENTITY_PROBABLE;
    else
        rec->identity_confidence = (int)EXT_IDENTITY_CANDIDATE;
    printf("[CANDIDATE_CHUNK] chunk_id=%llu evidence=0x%X bits=%d confidence=%s\n",
           (unsigned long long)rec->chunk_id, rec->identity_evidence, bits,
           ext_identity_confidence_name((ExtIdentityConfidence)rec->identity_confidence));
    fflush(stdout);
}

void ext_chunk_observe_bind_module(ExtChunkRecord *rec, uint64_t module_id, uint64_t load_id) {
    if (!rec) return;
    /* Only bind module_id when ≥2 evidence bits (A4 rule). */
    if (module_id) {
        ext_chunk_observe_add_evidence(rec, EXT_ID_EV_FIELD_POINTS_MODULE);
        if (popcount_u32(rec->identity_evidence) >= 2) {
            rec->module_id = module_id;
            if (load_id) rec->load_id = load_id;
            ext_chunk_observe_add_evidence(rec, EXT_ID_EV_LIFECYCLE_MATCH);
        } else {
            printf("[CANDIDATE_CHUNK] chunk_id=%llu defer_module_bind module_id=%llu "
                   "note=need_ge_2_evidence\n",
                   (unsigned long long)rec->chunk_id, (unsigned long long)module_id);
            fflush(stdout);
        }
    } else if (load_id) {
        rec->load_id = load_id;
    }
}

static void snapshot_words(ExtChunkRecord *rec) {
    size_t i;
    if (!rec || !g_chunk.uc) return;
    memset(rec->snap, 0, sizeof(rec->snap));
    rec->snap_valid = 1;
    for (i = 0; i < GWY_CHUNK_SNAP_BYTES; i += 4u) {
        uint32_t w = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)g_chunk.uc, rec->guest_base + (uint32_t)i,
                                     &w)) {
            memcpy(rec->snap + i, &w, 4);
        } else {
            rec->snap_valid = 0;
        }
    }
    if (rec->snap_valid) {
        uint32_t f4 = 0;
        memcpy(&f4, rec->snap + 4, 4);
        rec->field_04_raw = f4;
    }
}

void ext_chunk_observe_snapshot(ExtChunkRecord *rec, GwyChunkState stage) {
    size_t i;
    if (!rec || !rec->guest_base) return;
    rec->state = stage;
    snapshot_words(rec);
    printf("[CHUNK_SNAPSHOT] stage=%s chunk_id=%llu base=0x%X size=%u origin=%s "
           "field_04=0x%X snap_valid=%d",
           gwy_chunk_state_name(stage), (unsigned long long)rec->chunk_id, rec->guest_base,
           rec->size, gwy_chunk_origin_name(rec->origin), rec->field_04_raw, rec->snap_valid);
    if (rec->snap_valid) {
        for (i = 0; i < GWY_CHUNK_SNAP_BYTES; i += 4u) {
            uint32_t w = 0;
            memcpy(&w, rec->snap + i, 4);
            printf(" offset_%02X=0x%08X", (unsigned)i, w);
        }
    }
    printf("\n");
    fflush(stdout);
    if (stage == GWY_CHUNK_STATE_FIRST_SEEN && rec->field_04_raw)
        rec->field_04_at_first_seen = 1;
}

#ifdef GWY_HAVE_UNICORN
static void on_chunk_mem_write(uc_engine *uc,
                               uc_mem_type type,
                               uint64_t address,
                               int size,
                               int64_t value,
                               void *user_data) {
    ExtChunkRecord *rec = (ExtChunkRecord *)user_data;
    uint32_t addr = (uint32_t)address;
    uint32_t off;
    uint32_t pc = 0;
    char writer_mod[96];
    uint32_t writer_off = 0;
    uint64_t writer_mid = 0;
    ModuleRegistry *reg;
    (void)type;

    if (!rec || !rec->guest_base || addr < rec->guest_base) return;
    if (addr >= rec->guest_base + rec->size) return;
    off = addr - rec->guest_base;
    /* Only care about chunk header region (observe field_04 provenance). */
    if (off >= GWY_CHUNK_SNAP_BYTES) return;
    g_chunk.any_scalar_write = 1;

    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    {
        const GwyLoadedModule *m;
        uint32_t a = pc & ~1u;
        writer_mod[0] = '\0';
        reg = gwy_ext_loader_bound_registry();
        m = reg ? module_registry_find_by_code_addr(reg, a) : NULL;
        if (m) {
            writer_mid = m->module_id;
            snprintf(writer_mod, sizeof(writer_mod), "%s",
                     m->origin == MODULE_ORIGIN_DSM
                         ? "dsm:cfunction.ext"
                         : (m->resolved_name[0] ? m->resolved_name : m->requested_name));
            if (m->map.guest_code_base && a >= m->map.guest_code_base)
                writer_off = a - m->map.guest_code_base;
        } else {
            snprintf(writer_mod, sizeof(writer_mod), "%s", "unknown");
        }
    }

    if (off <= 0x4u && off + (uint32_t)size > 0x4u) {
        rec->field_04_write_seen = 1;
        if (size == 4 && off == 0x4u)
            rec->field_04_raw = (uint32_t)value;
        if (reg && rec->field_04_raw > 0x10000u) {
            const GwyLoadedModule *tgt =
                module_registry_find_by_code_addr(reg, rec->field_04_raw & ~1u);
            if (tgt) {
                LauncherError err;
                module_registry_set_chunk_field_04(reg, tgt->module_id, rec->field_04_raw, &err);
                ext_chunk_observe_bind_module(rec, tgt->module_id, 0);
            }
        }
    }

    printf("[CHUNK_WRITE] chunk_id=%llu dst_offset=0x%X size=%d writer_module=%s "
           "writer_offset=0x%X source_address=0x%X write_kind=scalar value=0x%X "
           "writer_module_id=%llu\n",
           (unsigned long long)rec->chunk_id, off, size, writer_mod, writer_off, addr,
           (uint32_t)value, (unsigned long long)writer_mid);
    fflush(stdout);
}

static void arm_chunk_watch(ExtChunkRecord *rec) {
    uc_err ue;
    uint32_t watch_len;
    if (!rec || !g_chunk.uc || !rec->guest_base || !rec->size) return;
    if (!ext_chunk_provenance_enabled()) return;
    /* Cap hook range to header: full-size watches on large allocs stall DSM. */
    watch_len = rec->size;
    if (watch_len > GWY_CHUNK_SNAP_BYTES) watch_len = GWY_CHUNK_SNAP_BYTES;
    if (rec->watch_armed) {
        uc_hook_del((uc_engine *)g_chunk.uc, (uc_hook)rec->write_hook);
        rec->watch_armed = 0;
        rec->write_hook = 0;
    }
    {
        uc_hook hk = 0;
        ue = uc_hook_add((uc_engine *)g_chunk.uc, &hk, UC_HOOK_MEM_WRITE, (void *)on_chunk_mem_write,
                         rec, (uint64_t)rec->guest_base,
                         (uint64_t)rec->guest_base + (uint64_t)watch_len - 1ull);
        if (ue == UC_ERR_OK) {
            rec->write_hook = (unsigned long long)hk;
            rec->watch_armed = 1;
            printf("[CHUNK_WRITE] watch_armed chunk_id=%llu base=0x%X size=%u watch_len=%u\n",
                   (unsigned long long)rec->chunk_id, rec->guest_base, rec->size, watch_len);
            fflush(stdout);
        }
    }
}
#else
static void arm_chunk_watch(ExtChunkRecord *rec) {
    (void)rec;
}
#endif

ExtChunkRecord *ext_chunk_observe_register(uint32_t guest_base,
                                           uint32_t size,
                                           GwyChunkOrigin origin,
                                           uint32_t creator_pc,
                                           uint64_t creator_module_id) {
    ExtChunkRecord *rec;
    if (!guest_base || !size) return NULL;
    rec = ext_chunk_observe_find(guest_base);
    if (!rec) {
        if (g_chunk.count >= GWY_CHUNK_OBS_MAX) return NULL;
        rec = &g_chunk.records[g_chunk.count++];
        memset(rec, 0, sizeof(*rec));
        rec->chunk_id = ++g_chunk.next_id;
        rec->guest_base = guest_base;
        rec->size = size;
        rec->origin = origin;
        rec->creator_pc = creator_pc;
        rec->creator_module_id = creator_module_id;
        rec->state = GWY_CHUNK_STATE_FIRST_SEEN;
        printf("[CHUNK_RECORD] chunk_id=%llu base=0x%X size=%u origin=%s creator_pc=0x%X "
               "creator_module_id=%llu\n",
               (unsigned long long)rec->chunk_id, guest_base, size, gwy_chunk_origin_name(origin),
               creator_pc, (unsigned long long)creator_module_id);
        fflush(stdout);
        arm_chunk_watch(rec);
        ext_chunk_observe_snapshot(rec, GWY_CHUNK_STATE_FIRST_SEEN);
    } else {
        if (size > rec->size) rec->size = size;
        if (origin != GWY_CHUNK_ORIGIN_UNKNOWN && rec->origin == GWY_CHUNK_ORIGIN_UNKNOWN)
            rec->origin = origin;
        if (origin == GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C) rec->origin = origin;
        arm_chunk_watch(rec);
    }
    return rec;
}

void ext_chunk_observe_on_alloc(uint32_t guest_addr, uint32_t size) {
    uint32_t pc = 0;
    uint64_t mid = 0;
    if (!ext_chunk_provenance_enabled()) return;
    if (!guest_addr || size < 0x2Cu) return;
    /* Skip huge buffers (RW images): not mrc_extChunk; watching them stalls live runs. */
    if (size > 0x200u) {
        printf("[CHUNK_RECORD] skip_large_alloc addr=0x%X size=%u note=not_chunk_candidate\n",
               guest_addr, size);
        fflush(stdout);
        return;
    }
#ifdef GWY_HAVE_UNICORN
    if (g_chunk.uc) uc_reg_read((uc_engine *)g_chunk.uc, UC_ARM_REG_PC, &pc);
#endif
    {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m =
            reg && pc ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
        if (m) mid = m->module_id;
    }
    ext_chunk_observe_register(guest_addr, size, GWY_CHUNK_ORIGIN_GUEST_ALLOCATED, pc, mid);
}

void ext_chunk_observe_on_p_plus_0c(uint32_t chunk_base, uint32_t p_base) {
    ExtChunkRecord *rec;
    (void)p_base;
    if (!chunk_base) return;
    if (!ext_chunk_provenance_enabled()) return;
    rec = ext_chunk_observe_register(chunk_base, 0x40u, GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C, 0, 0);
    if (rec && rec->state == GWY_CHUNK_STATE_FIRST_SEEN && !rec->snap_valid)
        ext_chunk_observe_snapshot(rec, GWY_CHUNK_STATE_FIRST_SEEN);
}

void ext_chunk_observe_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    ExtChunkRecord *rec;
    uint32_t off;
    if (!ext_chunk_provenance_enabled()) return;
    if (!dst || !len) return;
    rec = ext_chunk_observe_find_covering(dst);
    if (!rec) return;
    off = dst - rec->guest_base;
    /* Only header overlap is relevant for field_04 provenance. */
    if (off >= GWY_CHUNK_SNAP_BYTES) return;
    g_chunk.any_block_copy = 1;
    rec->block_copy_seen = 1;
    if (off <= 0x4u && off + len > 0x4u) {
        rec->field_04_write_seen = 1;
        if (g_chunk.uc) {
            uint32_t f4 = 0;
            if (guest_memory_uc_peek_u32((struct uc_struct *)g_chunk.uc, rec->guest_base + 0x4u,
                                         &f4))
                rec->field_04_raw = f4;
        }
    }
    printf("[CHUNK_WRITE] chunk_id=%llu dst_offset=0x%X size=%u writer_module=br_memcpy "
           "writer_offset=0x0 source_address=0x%X write_kind=block_copy src=0x%X\n",
           (unsigned long long)rec->chunk_id, off, len, dst, src);
    fflush(stdout);
}

static const char *classify_provenance_case(ExtChunkRecord *rec,
                                           uint32_t dsm_chunk_base,
                                           int identity_ok) {
    if (!identity_ok) return "D"; /* wrong-chunk-identity */
    if (!rec) return "E";         /* missing-chunk-bootstrap */
    if (rec->field_04_at_first_seen) return "C"; /* static-template */
    if (rec->block_copy_seen && !g_chunk.any_scalar_write) return "B"; /* block-copy-not-observed
                                                                         as scalar — still saw
                                                                         block */
    if (rec->field_04_write_seen) return "A"; /* observer caught write (or would have been late) */
    if (rec->field_04_raw && !rec->field_04_write_seen) return "A"; /* present without watched write
                                                                      → late or template-ish */
    if (rec->field_04_raw > 0x10000u) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        if (reg && !module_registry_find_by_code_addr(reg, rec->field_04_raw & ~1u))
            return "F"; /* corrupt / bad VA */
    }
    if (!rec->field_04_raw && !rec->field_04_write_seen && !rec->block_copy_seen) return "E";
    (void)dsm_chunk_base;
    return "G";
}

void ext_chunk_observe_on_dsm_select(uint32_t chunk_base,
                                     uint32_t field_04,
                                     uint32_t dsm_target,
                                     const char *caller_module,
                                     uint32_t caller_offset) {
    ExtChunkRecord *rec;
    ExtChunkRecord *by_dsm;
    int identity_ok = 1;
    const char *via = "unknown";
    const char *case_id;
    uint32_t f4 = field_04;
    uint32_t f4n;
    const char *fclass;
    const GwyLoadedModule *tgt = NULL;
    ModuleRegistry *reg;
    uint32_t tgt_off = 0;

    if (!ext_chunk_provenance_enabled()) return;

    rec = chunk_base ? ext_chunk_observe_find(chunk_base) : NULL;
    by_dsm = chunk_base ? ext_chunk_observe_find_covering(chunk_base) : NULL;
    if (rec && by_dsm && rec != by_dsm) identity_ok = 0;
    if (chunk_base && !rec && by_dsm) {
        rec = by_dsm;
        identity_ok = (by_dsm->guest_base == chunk_base);
    }
    if (chunk_base && !rec) {
        /* Late bind: DSM object was not seen at alloc (large-alloc skip / observer-late). */
        rec = ext_chunk_observe_register(chunk_base, 0x40u, GWY_CHUNK_ORIGIN_UNKNOWN, 0, 0);
        identity_ok = rec ? 1 : 0;
        if (!rec) {
            printf("[CHUNK_PTR_TRACE] caller_module=%s caller_offset=0x%X chunk_reg=? "
                   "chunk_base=0x%X field_04=0x%X via=unknown evidence=OBSERVED "
                   "wrong_chunk_identity=1 note=no_ExtChunkRecord\n",
                   caller_module ? caller_module : "unknown", caller_offset, chunk_base, field_04);
            fflush(stdout);
        }
    }
    if (rec) {
        if (rec->origin == GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C) via = "P_plus_0C";
        else if (rec->origin == GWY_CHUNK_ORIGIN_GUEST_ALLOCATED) via = "guest_alloc";
        else if (rec->origin == GWY_CHUNK_ORIGIN_UNKNOWN) via = "dsm_select_late_bind";
        else via = "module_table";
        printf("[CHUNK_PTR_TRACE] caller_module=%s caller_offset=0x%X chunk_reg=? "
               "chunk_base=0x%X field_04=0x%X via=%s evidence=OBSERVED "
               "wrong_chunk_identity=%d chunk_id=%llu\n",
               caller_module ? caller_module : "unknown", caller_offset, chunk_base,
               rec->field_04_raw ? rec->field_04_raw : field_04, via, identity_ok ? 0 : 1,
               (unsigned long long)rec->chunk_id);
        fflush(stdout);
        ext_chunk_observe_snapshot(rec, GWY_CHUNK_STATE_BEFORE_SELECT);
        {
            uint32_t snap_f4 = rec->field_04_raw;
            ext_chunk_observe_snapshot(rec, GWY_CHUNK_STATE_AFTER_SELECT);
            if (field_04 && snap_f4 && field_04 != snap_f4) {
                printf("[CHUNK_FIELD_04] snap_mismatch snap=0x%X dsm_select=0x%X "
                       "note=wrong_chunk_or_mutated\n",
                       snap_f4, field_04);
                fflush(stdout);
                identity_ok = 0;
            }
            if (field_04) rec->field_04_raw = field_04;
        }
        f4 = rec->field_04_raw ? rec->field_04_raw : field_04;
        /* DSM received this candidate pointer — one evidence bit only. */
        ext_chunk_observe_add_evidence(rec, EXT_ID_EV_DSM_RECEIVED);
        if (!identity_ok) {
            printf("[CANDIDATE_CHUNK] chunk_id=%llu candidate_mismatch confidence=%s "
                   "note=not_confirmed_mrc_extChunk\n",
                   (unsigned long long)rec->chunk_id,
                   ext_identity_confidence_name((ExtIdentityConfidence)rec->identity_confidence));
            fflush(stdout);
        }
    }

    f4n = f4 & ~1u;
    fclass = ext_chunk_field_04_class(f4);
    reg = gwy_ext_loader_bound_registry();
    if (reg && f4n) tgt = module_registry_find_by_code_addr(reg, f4n);
    if (tgt && tgt->map.guest_code_base && f4n >= tgt->map.guest_code_base)
        tgt_off = f4n - tgt->map.guest_code_base;

    printf("[CHUNK_FIELD_04] raw=0x%X normalized=0x%X class=%s target_module=%s "
           "target_offset=0x%X\n",
           f4, f4n, fclass,
           tgt ? (tgt->resolved_name[0] ? tgt->resolved_name : tgt->requested_name) : "unknown",
           tgt_off);
    fflush(stdout);

    {
        uint32_t first_pc = tgt ? tgt->entries.observed_first_pc : 0;
        int32_t delta = 0;
        if (first_pc) delta = (int32_t)(first_pc - f4n);
        printf("[ENTRY_TRANSFER] raw_target=0x%X normalized_target=0x%X first_executed_pc=0x%X "
               "delta=%d dsm_target=0x%X\n",
               f4, f4n, first_pc, (int)delta, dsm_target);
        fflush(stdout);
    }

    case_id = classify_provenance_case(rec, chunk_base, identity_ok);
    if (!g_chunk.case_emitted) {
        g_chunk.case_emitted = 1;
        printf("[CHUNK_PROVENANCE_CASE] case=%s "
               "legend=A_observer_late|B_block_copy|C_static_template|D_wrong_identity|"
               "E_missing_bootstrap|F_corrupt_field|G_unknown "
               "chunk_base=0x%X field_04=0x%X write_seen=%d block_copy=%d "
               "field_04_at_first_seen=%d identity_ok=%d evidence=OBSERVED "
               "phase6b_b_gate=%s\n",
               case_id, chunk_base, f4, rec ? rec->field_04_write_seen : 0,
               rec ? rec->block_copy_seen : 0, rec ? rec->field_04_at_first_seen : 0,
               identity_ok ? 1 : 0, (case_id[0] == 'E') ? "candidate_only" : "blocked");
        fflush(stdout);
    }

    if (!g_chunk.any_scalar_write && !g_chunk.any_block_copy) {
        printf("[CHUNK_WRITE] note=NONE_BEFORE_SELECT chunk_base=0x%X\n", chunk_base);
        fflush(stdout);
    }
}
