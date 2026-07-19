#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_MDI_ALLOC_MAX 48
#define GWY_MDI_PATH_INSNS 48
#define GWY_MDI_GLOBAL_MAX 16
#define GWY_MDI_HASH_CAP 4096

typedef struct {
    uint32_t base;
    uint32_t size;
    char kind[12];
    char allocator[8];
    char module[96];
    uint32_t caller_offset;
    int logged;
} MdiAlloc;

typedef struct {
    void *uc;
    ExtImageLayout layout;
    int layout_logged;
    int responsibility_logged;
    int identity_logged;
    int helper_vs_entry_logged;
    int class_logged;
    int snap_decompress;
    int snap_loadcode;
    int snap_before_entry;
    int reloc_logged;
    int entry_seen;
    int helper_seen;
    int entry_before_cfn;
    int saw_rw_alloc;
    int saw_template_copy;
    int saw_r9_zero_load;
    int saw_r9_branch;
    int entry_fault;
    int xt_partial;

    uint64_t focus_mid;
    char focus_module[96];
    uint32_t code_base;
    uint32_t code_size;
    uint32_t entry_pc;
    uint32_t entry_off;
    uint32_t entry_r0;
    uint32_t entry_r9;
    uint32_t helper_r9;
    uint32_t helper_addr;
    uint32_t p_guest;
    uint32_t p_len;
    uint32_t p_er_rw;
    uint32_t p_er_len;
    uint32_t allocated_rw_base;
    uint32_t allocated_rw_size;
    uint32_t registry_rw_base;
    uint32_t registry_rw_size;
    uint32_t derived_rw_base;
    uint32_t dsm_rw_base;
    uint32_t dsm_rw_size;
    uint32_t sample_count;
    uint32_t global_count;
    uint32_t first_global_imm;
    uint32_t first_global_value;
    int first_global_logged;
    char helper_r9_class[16];
    char entry_r9_class[16];
    char source_class[12];
    ExtDataInitClass last_class;
    MdiAlloc allocs[GWY_MDI_ALLOC_MAX];
    int alloc_count;
} ModuleDataInitState;

static ModuleDataInitState g_mdi;

void ext_module_data_init_reset(void) { memset(&g_mdi, 0, sizeof(g_mdi)); }

int ext_module_data_init_enabled(void) {
    const char *env = getenv("GWY_MODULE_DATA_INIT");
    return env && env[0] == '1';
}

void ext_module_data_init_bind_uc(void *uc) { g_mdi.uc = uc; }

const char *ext_data_init_class_name(ExtDataInitClass c) {
    switch (c) {
        case DATA_INIT_MISSING_RW_TEMPLATE_INITIALIZATION:
            return "MISSING_RW_TEMPLATE_INITIALIZATION";
        case DATA_INIT_MISSING_BSS_INITIALIZATION:
            return "MISSING_BSS_INITIALIZATION";
        case DATA_INIT_MISSING_DATA_RELOCATION:
            return "MISSING_DATA_RELOCATION";
        case DATA_INIT_MISSING_MODULE_R9_SWITCH:
            return "MISSING_MODULE_R9_SWITCH";
        case DATA_INIT_REGISTRY_DATA_METADATA_MISSING:
            return "REGISTRY_DATA_METADATA_MISSING";
        case DATA_INIT_MISSING_PRE_ENTRY_INITIALIZATION:
            return "MISSING_PRE_ENTRY_INITIALIZATION";
        default:
            return "UNKNOWN";
    }
}

ExtDataInitClass ext_module_data_init_last_class(void) { return g_mdi.last_class; }

static const char *layout_ev_name(GwyExtLayoutEvidence e) {
    switch (e) {
        case GWY_LAYOUT_EV_DOCUMENTED: return "DOCUMENTED";
        case GWY_LAYOUT_EV_FORMAT_CROSS_TARGET: return "FORMAT_CROSS_TARGET";
        case GWY_LAYOUT_EV_DISASM_DERIVED: return "DISASM_DERIVED";
        case GWY_LAYOUT_EV_OBSERVED: return "OBSERVED";
        default: return "UNKNOWN";
    }
}

static int name_is_dsm(const GwyLoadedModule *m) {
    if (!m) return 0;
    if (m->origin == MODULE_ORIGIN_DSM) return 1;
    if (m->requested_name[0] && strstr(m->requested_name, "dsm:")) return 1;
    return 0;
}

/* Focus = primary game EXT (robotol / aliased cfunction), not mrc_loader or DSM. */
static int name_is_nested_ext(const GwyLoadedModule *m) {
    const char *n;
    if (!m) return 0;
    if (name_is_dsm(m)) return 0;
    n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    if (!n || !n[0]) return 0;
    if (strstr(n, "mrc_loader")) return 0;
    if (strstr(n, "robotol")) return 1;
    if (m->origin == MODULE_ORIGIN_MRP_MEMBER && strstr(n, "cfunction")) return 1;
    return 0;
}

static void refresh_registry_rw(const GwyLoadedModule *m) {
    ModuleRegistry *reg;
    size_t i;
    if (m) {
        g_mdi.registry_rw_base = m->map.guest_rw_base;
        g_mdi.registry_rw_size = m->map.guest_rw_size;
        if (m->map.guest_code_base) {
            g_mdi.code_base = m->map.guest_code_base;
            g_mdi.code_size = m->map.guest_code_size;
        }
    } else if (g_mdi.focus_mid) {
        reg = gwy_ext_loader_bound_registry();
        m = reg ? module_registry_find_by_id(reg, g_mdi.focus_mid) : NULL;
        if (m) {
            g_mdi.registry_rw_base = m->map.guest_rw_base;
            g_mdi.registry_rw_size = m->map.guest_rw_size;
        }
    }
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *x = &reg->modules[i];
        if (name_is_dsm(x) && x->map.guest_rw_base) {
            g_mdi.dsm_rw_base = x->map.guest_rw_base;
            g_mdi.dsm_rw_size = x->map.guest_rw_size;
        }
    }
}

static void classify_r9(uint32_t r9, char out[16]) {
    if (!r9) {
        snprintf(out, 16, "%s", "ZERO");
        return;
    }
    /* Prefer DSM ownership before any mis-attributed allocated_rw. */
    if (g_mdi.dsm_rw_base && r9 == g_mdi.dsm_rw_base) {
        snprintf(out, 16, "%s", "DSM_RW");
        return;
    }
    if (g_mdi.p_er_rw && r9 == g_mdi.p_er_rw) {
        snprintf(out, 16, "%s", "MODULE_RW");
        return;
    }
    if (g_mdi.allocated_rw_base && r9 == g_mdi.allocated_rw_base &&
        !(g_mdi.dsm_rw_base && g_mdi.allocated_rw_base == g_mdi.dsm_rw_base)) {
        snprintf(out, 16, "%s", "MODULE_RW");
        return;
    }
    if (g_mdi.registry_rw_base && r9 == g_mdi.registry_rw_base) {
        snprintf(out, 16, "%s", "MODULE_RW");
        return;
    }
    snprintf(out, 16, "%s", "UNKNOWN");
}

static void peek_p_fields(void) {
    if (!g_mdi.uc || !g_mdi.p_guest) return;
    {
        uint32_t er = 0, len = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)g_mdi.uc, g_mdi.p_guest, &er))
            g_mdi.p_er_rw = er;
        if (guest_memory_uc_peek_u32((struct uc_struct *)g_mdi.uc, g_mdi.p_guest + 4u, &len))
            g_mdi.p_er_len = len;
    }
    /* Module-owned ER_RW from P wins over any DSM-attributed allocated_rw. */
    if (g_mdi.p_er_rw) {
        g_mdi.allocated_rw_base = g_mdi.p_er_rw;
        g_mdi.allocated_rw_size = g_mdi.p_er_len;
        g_mdi.derived_rw_base = g_mdi.p_er_rw;
        g_mdi.saw_rw_alloc = 1;
    } else if (g_mdi.allocated_rw_base &&
               !(g_mdi.dsm_rw_base && g_mdi.allocated_rw_base == g_mdi.dsm_rw_base)) {
        g_mdi.derived_rw_base = g_mdi.allocated_rw_base;
    }
}

static uint32_t hash_region(void *uc, uint32_t base, uint32_t size) {
    uint32_t h = 2166136261u;
    uint32_t off;
    uint32_t lim = size;
    if (!uc || !base || !size) return 0;
    if (lim > GWY_MDI_HASH_CAP) lim = GWY_MDI_HASH_CAP;
    for (off = 0; off + 4u <= lim; off += 4u) {
        uint32_t v = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, base + off, &v)) break;
        h ^= v;
        h *= 16777619u;
    }
    return h;
}

static uint32_t count_bss_nonzero(void *uc, uint32_t base, uint32_t size) {
    uint32_t off, n = 0, lim = size;
    if (!uc || !base || !size) return 0;
    if (lim > GWY_MDI_HASH_CAP) lim = GWY_MDI_HASH_CAP;
    for (off = 0; off + 4u <= lim; off += 4u) {
        uint32_t v = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, base + off, &v)) break;
        if (v) n++;
    }
    return n;
}

static void emit_responsibility(void) {
    if (g_mdi.responsibility_logged || !ext_module_data_init_enabled()) return;
    g_mdi.responsibility_logged = 1;
    printf("[DATA_INIT_RESPONSIBILITY] "
           "CODE_alloc=DSM/loadCode status=%s evidence=OBSERVED "
           "RW_template_copy=guest_mr_c_function_load status=%s evidence=DOCUMENTED "
           "BSS_zero=guest_mr_c_function_load status=%s evidence=DOCUMENTED "
           "relocation=UNKNOWN status=unknown evidence=UNKNOWN "
           "R9_set=helper_path_fixR9 status=%s evidence=DOCUMENTED "
           "entry_call=DSM_BLX status=confirmed evidence=OBSERVED "
           "host_duty=P_alloc_and_host_helper_R9_only note=nested_RW_not_host "
           "note=no_guest_mutation\n",
           g_mdi.code_base ? "observed" : "pending",
           g_mdi.saw_template_copy ? "observed_copy" : "not_observed",
           (g_mdi.p_er_rw && g_mdi.p_er_len) ? "p_filled" : "not_observed",
           g_mdi.helper_seen ? "helper_r9_seen" : "pending");
    fflush(stdout);
}

static void emit_layout(const GwyLoadedModule *m) {
    ExtImageLayout *L = &g_mdi.layout;
    const char *mod;
    if (g_mdi.layout_logged) return;
    g_mdi.layout_logged = 1;
    memset(L, 0, sizeof(*L));
    if (m) {
        L->image_size = m->extracted_size ? m->extracted_size : m->unpacked_size;
        L->image_size_ev = L->image_size ? GWY_LAYOUT_EV_OBSERVED : GWY_LAYOUT_EV_UNKNOWN;
        L->code_offset = 0;
        L->code_size = m->map.guest_code_size ? m->map.guest_code_size : g_mdi.code_size;
        L->code_ev = L->code_size ? GWY_LAYOUT_EV_OBSERVED : GWY_LAYOUT_EV_UNKNOWN;
    }
    /* DOCUMENTED: RW template follows RO; sizes from guest helpers / P after load. */
    if (g_mdi.p_er_len) {
        L->rw_template_size = g_mdi.allocated_rw_size ? g_mdi.allocated_rw_size : g_mdi.p_er_len;
        L->rw_template_ev = GWY_LAYOUT_EV_DOCUMENTED;
        /* BSS portion unknown without rw_lenOnly; leave UNKNOWN unless sizes differ. */
        L->bss_size = 0;
        L->bss_ev = GWY_LAYOUT_EV_UNKNOWN;
    } else {
        L->rw_template_offset = 0;
        L->rw_template_size = 0;
        L->rw_template_ev = GWY_LAYOUT_EV_UNKNOWN;
        L->bss_ev = GWY_LAYOUT_EV_UNKNOWN;
    }
    L->relocation_ev = GWY_LAYOUT_EV_UNKNOWN;
    mod = g_mdi.focus_module[0] ? g_mdi.focus_module : (m ? (m->resolved_name[0] ? m->resolved_name
                                                                                 : m->requested_name)
                                                         : "?");
    printf("[EXT_IMAGE_LAYOUT] module=%s image_size=%u image_ev=%s "
           "code_offset=%u code_size=%u code_ev=%s "
           "rw_template_offset=%u rw_template_size=%u rw_ev=%s "
           "bss_size=%u bss_ev=%s reloc_offset=%u reloc_count=%u reloc_ev=%s "
           "note=no_hardcoded_section_cuts evidence=mixed\n",
           mod, L->image_size, layout_ev_name(L->image_size_ev), L->code_offset, L->code_size,
           layout_ev_name(L->code_ev), L->rw_template_offset, L->rw_template_size,
           layout_ev_name(L->rw_template_ev), L->bss_size, layout_ev_name(L->bss_ev),
           L->relocation_offset, L->relocation_count, layout_ev_name(L->relocation_ev));
    fflush(stdout);
}

static void emit_snapshot(const char *stage) {
    uint32_t rw = g_mdi.derived_rw_base ? g_mdi.derived_rw_base : g_mdi.allocated_rw_base;
    uint32_t sz = g_mdi.allocated_rw_size ? g_mdi.allocated_rw_size : g_mdi.p_er_len;
    uint32_t h = 0, bss_nz = 0;
    const char *match = "unknown";
    if (!ext_module_data_init_enabled()) return;
    if (rw && sz && g_mdi.uc) {
        h = hash_region(g_mdi.uc, rw, sz);
        bss_nz = count_bss_nonzero(g_mdi.uc, rw, sz);
        if (g_mdi.saw_template_copy) match = "yes";
        else if (!rw) match = "no";
    } else if (!rw) {
        match = "no";
    }
    printf("[MODULE_DATA_SNAPSHOT] stage=%s module=%s rw_base=0x%X rw_size=%u rw_hash=0x%08X "
           "bss_nonzero_count=%u template_match=%s evidence=OBSERVED note=no_guest_mutation\n",
           stage, g_mdi.focus_module[0] ? g_mdi.focus_module : "?", rw, sz, h, bss_nz, match);
    fflush(stdout);
}

static void emit_r9_contract(const char *stage, const char *module, uint32_t r9, uint32_t er_rw) {
    char cls[16];
    classify_r9(r9, cls);
    printf("[R9_CONTRACT] stage=%s module=%s r9=0x%X er_rw=0x%X classification=%s "
           "evidence=OBSERVED\n",
           stage, module && module[0] ? module : "?", r9, er_rw, cls);
    fflush(stdout);
}

static void emit_rw_identity(const GwyLoadedModule *m) {
    uint32_t decl_rw = 0, decl_bss = 0;
    const char *src = "UNKNOWN";
    if (g_mdi.identity_logged) return;
    g_mdi.identity_logged = 1;
    refresh_registry_rw(m);
    peek_p_fields();
    if (g_mdi.p_er_len) {
        decl_rw = g_mdi.p_er_len; /* total ER_RW+ZI until rw_lenOnly known */
        decl_bss = 0;
    }
    if (!g_mdi.derived_rw_base) {
        if (g_mdi.p_er_rw) g_mdi.derived_rw_base = g_mdi.p_er_rw;
        else if (g_mdi.allocated_rw_base)
            g_mdi.derived_rw_base = g_mdi.allocated_rw_base;
        else if (g_mdi.registry_rw_base)
            g_mdi.derived_rw_base = g_mdi.registry_rw_base;
    }
    /* A–F source classification */
    if (g_mdi.p_er_rw && g_mdi.entry_r9 && g_mdi.entry_r9 != g_mdi.p_er_rw &&
        g_mdi.dsm_rw_base && g_mdi.entry_r9 == g_mdi.dsm_rw_base) {
        src = "D"; /* RW exists (P) but entry-time R9 still DSM */
    } else if (decl_rw == 0 && !g_mdi.p_er_rw && !g_mdi.allocated_rw_base &&
               !g_mdi.registry_rw_base) {
        if (g_mdi.entry_seen && !g_mdi.helper_seen && !g_mdi.p_er_rw)
            src = "E";
        else
            src = "A";
    } else if (g_mdi.p_er_rw && !g_mdi.registry_rw_base) {
        src = "C";
    } else if ((g_mdi.allocated_rw_base || g_mdi.p_er_rw || g_mdi.registry_rw_base) &&
               !g_mdi.entry_r9) {
        src = "D";
    } else if (g_mdi.entry_before_cfn) {
        src = "E";
    } else {
        src = "F";
    }
    snprintf(g_mdi.source_class, sizeof(g_mdi.source_class), "%s", src);
    printf("[RW_BASE_IDENTITY] module=%s declared_rw_size=%u declared_bss_size=%u "
           "allocated_rw_base=0x%X registry_rw_base=0x%X entry_r9=0x%X derived_rw_base=0x%X "
           "source_class=%s p_er_rw=0x%X p_er_len=%u dsm_rw_base=0x%X evidence=OBSERVED "
           "note=A_decl0|B_parser|C_registry|D_entry_r9|E_self_init|F_field_id\n",
           g_mdi.focus_module[0] ? g_mdi.focus_module : "?", decl_rw, decl_bss,
           g_mdi.allocated_rw_base, g_mdi.registry_rw_base, g_mdi.entry_r9, g_mdi.derived_rw_base,
           src, g_mdi.p_er_rw, g_mdi.p_er_len, g_mdi.dsm_rw_base);
    fflush(stdout);
}

static void emit_helper_vs_entry(void) {
    int same;
    if (g_mdi.helper_vs_entry_logged) return;
    if (!g_mdi.entry_seen) return;
    g_mdi.helper_vs_entry_logged = 1;
    classify_r9(g_mdi.helper_r9, g_mdi.helper_r9_class);
    classify_r9(g_mdi.entry_r9, g_mdi.entry_r9_class);
    same = (g_mdi.helper_r9 == g_mdi.entry_r9) ? 1 : 0;
    printf("[R9_HELPER_VS_ENTRY] module=%s helper_r9=0x%X helper_class=%s "
           "entry_r9=0x%X entry_class=%s same=%d helper_seen=%d evidence=OBSERVED "
           "note=primary_discriminant\n",
           g_mdi.focus_module[0] ? g_mdi.focus_module : "?", g_mdi.helper_r9, g_mdi.helper_r9_class,
           g_mdi.entry_r9, g_mdi.entry_r9_class, same, g_mdi.helper_seen);
    fflush(stdout);
}

static void emit_reloc_observe(const GwyLoadedModule *m) {
    uint32_t helper = 0;
    const char *status = "UNKNOWN";
    if (g_mdi.reloc_logged || !m) return;
    g_mdi.reloc_logged = 1;
    helper = m->entries.registered_helper;
    if (helper && m->map.guest_code_base) {
        uint32_t off = (helper & ~1u) - m->map.guest_code_base;
        if ((helper & ~1u) >= m->map.guest_code_base &&
            off < (m->map.guest_code_size ? m->map.guest_code_size : 0x100000u))
            status = "APPLIED"; /* helper lands inside mapped CODE — OBSERVED legality */
        else
            status = "SKIPPED";
    }
    printf("[EXT_RELOCATION] module=%s entry_index=0 target_section=CODE "
           "before=0x0 after=0x%X type=helper_in_code status=%s "
           "registered_helper=0x%X evidence=OBSERVED note=no_reloc_applicator\n",
           g_mdi.focus_module[0] ? g_mdi.focus_module : "?", helper, status, helper);
    fflush(stdout);
}

static void classify_and_emit(void) {
    ExtDataInitClass c = DATA_INIT_UNKNOWN;
    const char *gate = "blocked";
    const char *next_fix = "none";
    const char *gev = "none";
    int entry_is_dsm =
        (g_mdi.dsm_rw_base && g_mdi.entry_r9 == g_mdi.dsm_rw_base) ||
        (strcmp(g_mdi.entry_r9_class, "DSM_RW") == 0);
    int entry_is_zero = !g_mdi.entry_r9 || strcmp(g_mdi.entry_r9_class, "ZERO") == 0;
    int entry_not_own =
        g_mdi.p_er_rw && g_mdi.entry_r9 && g_mdi.entry_r9 != g_mdi.p_er_rw;
    int helper_not_own =
        g_mdi.p_er_rw && g_mdi.helper_r9 && g_mdi.helper_r9 != g_mdi.p_er_rw;
    uint32_t own_alloc = g_mdi.allocated_rw_base;
    if (own_alloc && g_mdi.dsm_rw_base && own_alloc == g_mdi.dsm_rw_base) own_alloc = 0;

    if (g_mdi.class_logged) return;
    g_mdi.class_logged = 1;
    refresh_registry_rw(NULL);
    peek_p_fields();
    classify_r9(g_mdi.helper_r9, g_mdi.helper_r9_class);
    classify_r9(g_mdi.entry_r9, g_mdi.entry_r9_class);
    entry_is_dsm =
        (g_mdi.dsm_rw_base && g_mdi.entry_r9 == g_mdi.dsm_rw_base) ||
        (strcmp(g_mdi.entry_r9_class, "DSM_RW") == 0);
    entry_not_own = g_mdi.p_er_rw && g_mdi.entry_r9 && g_mdi.entry_r9 != g_mdi.p_er_rw;
    helper_not_own = g_mdi.p_er_rw && g_mdi.helper_r9 && g_mdi.helper_r9 != g_mdi.p_er_rw;
    emit_helper_vs_entry();
    emit_rw_identity(NULL);
    emit_responsibility();

    /*
     * Primary: module owns ER_RW (P filled) but entry/helper R9 still DSM or zero.
     * DOCUMENTED by fixR9 nested-ext contract.
     */
    if (g_mdi.p_er_rw && (entry_not_own || helper_not_own) && (entry_is_dsm || entry_is_zero)) {
        c = DATA_INIT_MISSING_MODULE_R9_SWITCH;
        next_fix = "MODULE_R9_SWITCH";
        gev = "DOCUMENTED";
    } else if (!g_mdi.p_er_rw && !own_alloc && g_mdi.entry_seen &&
               (entry_is_dsm || entry_is_zero)) {
        /* Module entry before mr_c_function_load filled P/ER_RW. */
        c = DATA_INIT_MISSING_PRE_ENTRY_INITIALIZATION;
        next_fix = "PRE_ENTRY_MR_C_FUNCTION_LOAD";
        gev = "DOCUMENTED";
    } else if (!own_alloc && !g_mdi.p_er_rw && g_mdi.entry_fault) {
        c = DATA_INIT_MISSING_RW_TEMPLATE_INITIALIZATION;
        next_fix = "RW_TEMPLATE_INIT";
        gev = "OBSERVED";
    } else if (g_mdi.p_er_rw && !g_mdi.registry_rw_base &&
               g_mdi.entry_r9 == g_mdi.p_er_rw) {
        c = DATA_INIT_REGISTRY_DATA_METADATA_MISSING;
        next_fix = "REGISTRY_RW_METADATA";
        gev = "OBSERVED";
    } else if (g_mdi.entry_r9 == 0 && g_mdi.entry_fault && g_mdi.dsm_rw_base) {
        c = DATA_INIT_MISSING_MODULE_R9_SWITCH;
        next_fix = "MODULE_R9_SWITCH";
        gev = "OBSERVED";
    } else {
        c = DATA_INIT_UNKNOWN;
        next_fix = "none";
        gev = "none";
    }
    gate = "blocked";
    g_mdi.last_class = c;
    printf("[DATA_INIT_CLASS] class=%s source_class=%s helper_r9=0x%X entry_r9=0x%X "
           "allocated_rw=0x%X registry_rw=0x%X p_er_rw=0x%X entry_before_cfn=%d "
           "saw_r9_zero_load=%d phase6b_b_gate=%s gate_evidence=%s "
           "next_allowed_fix=%s evidence=OBSERVED note=no_guest_mutation\n",
           ext_data_init_class_name(c), g_mdi.source_class[0] ? g_mdi.source_class : "?",
           g_mdi.helper_r9, g_mdi.entry_r9, g_mdi.allocated_rw_base, g_mdi.registry_rw_base,
           g_mdi.p_er_rw, g_mdi.entry_before_cfn, g_mdi.saw_r9_zero_load, gate, gev, next_fix);
    printf("[XT_DATA_INIT] target=jjfb declared_rw=%u allocated_rw=0x%X entry_r9=0x%X "
           "first_global_imm=0x%X first_global_value=0x%X xt_pattern=PARTIAL "
           "evidence=OBSERVED\n",
           g_mdi.p_er_len, g_mdi.p_er_rw ? g_mdi.p_er_rw : g_mdi.allocated_rw_base, g_mdi.entry_r9,
           g_mdi.first_global_imm, g_mdi.first_global_value);
    fflush(stdout);
}

void ext_module_data_init_on_code_image(uint32_t guest_addr, uint32_t size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_module_data_init_enabled() || !guest_addr) return;
    g_mdi.code_base = guest_addr;
    g_mdi.code_size = size;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, guest_addr) : NULL;
    if (m && name_is_nested_ext(m)) {
        g_mdi.focus_mid = m->module_id;
        snprintf(g_mdi.focus_module, sizeof(g_mdi.focus_module), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
        if (!g_mdi.snap_decompress) {
            g_mdi.snap_decompress = 1;
            emit_snapshot("AFTER_DECOMPRESS");
        }
        emit_layout(m);
        if (!g_mdi.snap_loadcode) {
            g_mdi.snap_loadcode = 1;
            emit_r9_contract("LOAD_RETURN", g_mdi.focus_module, 0, g_mdi.p_er_rw);
            emit_r9_contract("CACHE_SYNC", g_mdi.focus_module, 0, g_mdi.p_er_rw);
            emit_snapshot("AFTER_LOADCODE");
        }
        printf("[EXT_DATA_ALLOC] module=%s kind=CODE base=0x%X size=%u allocator=DSM "
               "caller_module=dsm caller_offset=0 evidence=OBSERVED\n",
               g_mdi.focus_module, guest_addr, size);
        fflush(stdout);
    } else if (m && name_is_dsm(m)) {
        refresh_registry_rw(m);
        printf("[EXT_DATA_ALLOC] module=%s kind=CODE base=0x%X size=%u allocator=DSM "
               "caller_module=host caller_offset=0 evidence=OBSERVED\n",
               m->resolved_name[0] ? m->resolved_name : m->requested_name, guest_addr, size);
        fflush(stdout);
    }
    emit_responsibility();
}

void ext_module_data_init_on_alloc(uint32_t guest_addr, uint32_t size) {
    MdiAlloc *a;
    const char *kind = "UNKNOWN";
    const char *alloc = "GUEST";
    ModuleRegistry *reg;
    const GwyLoadedModule *m = NULL;
    char mod[96];
    if (!ext_module_data_init_enabled() || !guest_addr || !size) return;
    reg = gwy_ext_loader_bound_registry();
    mod[0] = '\0';
    if (size == 20u || size == 0x14u) {
        kind = "P";
        alloc = "HOST";
    } else if (g_mdi.code_size && size == g_mdi.code_size) {
        kind = "CODE";
        alloc = "DSM";
    } else if (g_mdi.p_er_len && size == g_mdi.p_er_len) {
        kind = "RW";
        g_mdi.saw_rw_alloc = 1;
        g_mdi.allocated_rw_base = guest_addr;
        g_mdi.allocated_rw_size = size;
        g_mdi.derived_rw_base = guest_addr;
    } else if (size >= 0x100u && size <= 0x20000u && g_mdi.focus_mid) {
        /* Candidate nested ER_RW (DOCUMENTED: mrc_malloc(ER_RW_Length)). */
        kind = "RW";
        if (!g_mdi.allocated_rw_base) {
            g_mdi.saw_rw_alloc = 1;
            g_mdi.allocated_rw_base = guest_addr;
            g_mdi.allocated_rw_size = size;
            g_mdi.derived_rw_base = guest_addr;
        }
    }
    if (reg && g_mdi.focus_mid) m = module_registry_find_by_id(reg, g_mdi.focus_mid);
    if (m)
        snprintf(mod, sizeof(mod), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
    else if (g_mdi.focus_module[0])
        snprintf(mod, sizeof(mod), "%s", g_mdi.focus_module);
    else
        snprintf(mod, sizeof(mod), "%s", "?");

    if (g_mdi.alloc_count < GWY_MDI_ALLOC_MAX) {
        a = &g_mdi.allocs[g_mdi.alloc_count++];
        memset(a, 0, sizeof(*a));
        a->base = guest_addr;
        a->size = size;
        snprintf(a->kind, sizeof(a->kind), "%s", kind);
        snprintf(a->allocator, sizeof(a->allocator), "%s", alloc);
        snprintf(a->module, sizeof(a->module), "%s", mod);
        a->logged = 1;
    }
    printf("[EXT_DATA_ALLOC] module=%s kind=%s base=0x%X size=%u allocator=%s "
           "caller_module=? caller_offset=0 evidence=OBSERVED\n",
           mod, kind, guest_addr, size, alloc);
    fflush(stdout);
}

void ext_module_data_init_on_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    if (!ext_module_data_init_enabled() || !dst || !len) return;
    (void)src;
    /* Ignore DSM ER_RW template copies — do not attribute to nested focus module. */
    if (g_mdi.dsm_rw_base && dst == g_mdi.dsm_rw_base) return;
    if (g_mdi.allocated_rw_base && dst == g_mdi.allocated_rw_base &&
        !(g_mdi.dsm_rw_base && g_mdi.allocated_rw_base == g_mdi.dsm_rw_base)) {
        g_mdi.saw_template_copy = 1;
        printf("[EXT_DATA_ALLOC] module=%s kind=RW_TEMPLATE_COPY base=0x%X size=%u "
               "allocator=GUEST caller_module=? caller_offset=0 evidence=OBSERVED\n",
               g_mdi.focus_module[0] ? g_mdi.focus_module : "?", dst, len);
        fflush(stdout);
    } else if (g_mdi.focus_mid && g_mdi.code_base && src >= g_mdi.code_base &&
               src < g_mdi.code_base + (g_mdi.code_size ? g_mdi.code_size : 1u) && len >= 0x40u) {
        g_mdi.saw_template_copy = 1;
        if (!g_mdi.allocated_rw_base ||
            (g_mdi.dsm_rw_base && g_mdi.allocated_rw_base == g_mdi.dsm_rw_base)) {
            g_mdi.allocated_rw_base = dst;
            g_mdi.allocated_rw_size = len;
            g_mdi.derived_rw_base = dst;
            g_mdi.saw_rw_alloc = 1;
        }
        printf("[EXT_DATA_ALLOC] module=%s kind=RW_TEMPLATE_COPY base=0x%X size=%u "
               "allocator=GUEST caller_module=? caller_offset=0 src=0x%X evidence=OBSERVED\n",
               g_mdi.focus_module[0] ? g_mdi.focus_module : "?", dst, len, src);
        fflush(stdout);
    }
}

void ext_module_data_init_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                         uint32_t rw_base, uint32_t rw_size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_module_data_init_enabled() || !helper) return;
    g_mdi.helper_addr = helper;
    if (p_guest) {
        g_mdi.p_guest = p_guest;
        g_mdi.p_len = p_len;
    }
    if (rw_base) {
        g_mdi.allocated_rw_base = rw_base;
        g_mdi.allocated_rw_size = rw_size;
        g_mdi.derived_rw_base = rw_base;
        g_mdi.p_er_rw = rw_base;
        g_mdi.p_er_len = rw_size;
        g_mdi.saw_rw_alloc = 1;
    }
    peek_p_fields();
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
    if (!m && reg) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    if (m && name_is_nested_ext(m)) {
        g_mdi.focus_mid = m->module_id;
        snprintf(g_mdi.focus_module, sizeof(g_mdi.focus_module), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
        refresh_registry_rw(m);
        emit_layout(m);
        peek_p_fields();
        if (g_mdi.entry_seen) {
            g_mdi.class_logged = 0;
            g_mdi.helper_vs_entry_logged = 0;
            g_mdi.identity_logged = 0;
            classify_and_emit();
        }
    } else if (m && name_is_dsm(m)) {
        refresh_registry_rw(m);
    }
}

void ext_module_data_init_on_registered(uint64_t module_id, uint32_t helper) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_module_data_init_enabled() || !helper) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    if (m && name_is_nested_ext(m)) {
        g_mdi.focus_mid = module_id;
        snprintf(g_mdi.focus_module, sizeof(g_mdi.focus_module), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
        g_mdi.helper_addr = helper;
        refresh_registry_rw(m);
        emit_r9_contract("REGISTER", g_mdi.focus_module, 0, g_mdi.p_er_rw);
        emit_reloc_observe(m);
    }
}

void ext_module_data_init_on_mr_helper(uint32_t helper, uint32_t code, uint32_t p_guest,
                                       uint32_t r9) {
    if (!ext_module_data_init_enabled()) return;
    (void)code;
    (void)p_guest;
    if (!g_mdi.helper_seen) {
        g_mdi.helper_seen = 1;
        g_mdi.helper_addr = helper;
        g_mdi.helper_r9 = r9;
        if (p_guest && !g_mdi.p_guest) g_mdi.p_guest = p_guest;
        peek_p_fields();
        classify_r9(r9, g_mdi.helper_r9_class);
        emit_r9_contract("MR_HELPER", g_mdi.focus_module, r9, g_mdi.p_er_rw ? g_mdi.p_er_rw : r9);
        if (g_mdi.entry_seen) {
            g_mdi.helper_vs_entry_logged = 0;
            emit_helper_vs_entry();
            /* Refresh class once helper R9 is known (primary discriminant). */
            g_mdi.class_logged = 0;
            classify_and_emit();
        }
        return;
    }
    /* Update helper R9 if a later helper enter shows a clearer MODULE_RW. */
    if (r9 && r9 != g_mdi.helper_r9) {
        g_mdi.helper_r9 = r9;
        classify_r9(r9, g_mdi.helper_r9_class);
        emit_r9_contract("MR_HELPER", g_mdi.focus_module, r9, g_mdi.p_er_rw ? g_mdi.p_er_rw : r9);
        if (g_mdi.entry_seen) {
            g_mdi.helper_vs_entry_logged = 0;
            emit_helper_vs_entry();
        }
    }
}

void ext_module_data_init_on_entry_begin(uint32_t helper, uint32_t er_rw, uint32_t r9) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_module_data_init_enabled() || !helper) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
    if (!m && reg) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    /* Host DSM helper enter is not robotol helper-time R9. */
    if (!m || !name_is_nested_ext(m)) {
        if (er_rw) {
            refresh_registry_rw(m);
            if (!g_mdi.dsm_rw_base && er_rw) {
                g_mdi.dsm_rw_base = er_rw;
            }
        }
        return;
    }
    if (!g_mdi.helper_seen) {
        g_mdi.helper_seen = 1;
        g_mdi.helper_r9 = r9 ? r9 : er_rw;
        classify_r9(g_mdi.helper_r9, g_mdi.helper_r9_class);
        emit_r9_contract("MR_HELPER", g_mdi.focus_module[0] ? g_mdi.focus_module : "?",
                         g_mdi.helper_r9, er_rw);
    }
    if (er_rw && !g_mdi.p_er_rw) {
        g_mdi.p_er_rw = er_rw;
        g_mdi.derived_rw_base = er_rw;
    }
}

void ext_module_data_init_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target, uint32_t r0,
                                        uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                                        uint64_t from_mid, uint64_t to_mid,
                                        const char *target_relation) {
    ModuleRegistry *reg;
    const GwyLoadedModule *from_m, *to_m;
    const char *rel = target_relation ? target_relation : "unknown";
    uint32_t target_off = 0;
    int is_primary;
    (void)r1;
    (void)r2;
    (void)r3;
    (void)caller_pc;
    if (!ext_module_data_init_enabled() || !target) return;
    if (uc) g_mdi.uc = uc;
    reg = gwy_ext_loader_bound_registry();
    from_m = reg ? module_registry_find_by_id(reg, from_mid) : NULL;
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    if (!to_m || !name_is_nested_ext(to_m)) return;
    if (strcmp(rel, "helper") == 0) return;

    g_mdi.focus_mid = to_mid;
    snprintf(g_mdi.focus_module, sizeof(g_mdi.focus_module), "%s",
             to_m->resolved_name[0] ? to_m->resolved_name : to_m->requested_name);
    refresh_registry_rw(to_m);
    peek_p_fields();
    if (to_m->map.guest_code_base && (target & ~1u) >= to_m->map.guest_code_base)
        target_off = (target & ~1u) - to_m->map.guest_code_base;

    is_primary = (strcmp(rel, "internal_entry") == 0) ||
                 (strcmp(rel, "trampoline") == 0 && target_off <= 0x40u);
    if (!is_primary && !(to_m->entries.registered_helper &&
                         (target & ~1u) != (to_m->entries.registered_helper & ~1u)))
        return;

    if (!g_mdi.entry_seen) {
        g_mdi.entry_seen = 1;
        g_mdi.entry_pc = target;
        g_mdi.entry_off = target_off;
        g_mdi.entry_r0 = r0;
        g_mdi.entry_r9 = r9;
        if (!g_mdi.p_er_rw) g_mdi.entry_before_cfn = 1;
        classify_r9(r9, g_mdi.entry_r9_class);
        emit_r9_contract("MODULE_ENTRY", g_mdi.focus_module, r9, g_mdi.p_er_rw);
        if (!g_mdi.snap_before_entry) {
            g_mdi.snap_before_entry = 1;
            emit_snapshot("BEFORE_MODULE_ENTRY");
        }
        emit_layout(to_m);
        emit_reloc_observe(to_m);
        emit_rw_identity(to_m);
        emit_helper_vs_entry();
        /* Provisional class for live gate if killed before fault. */
        if (!g_mdi.class_logged) classify_and_emit();
    } else if (strcmp(rel, "internal_entry") == 0) {
        g_mdi.entry_pc = target;
        g_mdi.entry_off = target_off;
        g_mdi.entry_r0 = r0;
        if (r9) {
            g_mdi.entry_r9 = r9;
            classify_r9(r9, g_mdi.entry_r9_class);
        }
    }
    (void)from_m;
}

void ext_module_data_init_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                  uint32_t cpsr) {
    int thumb, rt = -1, rn = -1, is_load = 0;
    uint32_t word = 0, imm = 0;
    if (!ext_module_data_init_enabled() || !regs) return;
    if (uc) g_mdi.uc = uc;

    /* Helper PC enter: capture R9 if not yet seen. */
    if (g_mdi.helper_addr && (pc & ~1u) == (g_mdi.helper_addr & ~1u) && !g_mdi.helper_seen) {
        if (g_mdi.p_guest && regs[0] == g_mdi.p_guest)
            ext_module_data_init_on_mr_helper(g_mdi.helper_addr, regs[1], g_mdi.p_guest, regs[9]);
    }

    if (!g_mdi.entry_seen || g_mdi.focus_mid != module_id) return;
    if (g_mdi.sample_count >= GWY_MDI_PATH_INSNS) return;
    g_mdi.sample_count++;

    if (!g_mdi.entry_r9 && regs[9]) {
        g_mdi.entry_r9 = regs[9];
        classify_r9(regs[9], g_mdi.entry_r9_class);
        emit_r9_contract("MODULE_ENTRY", g_mdi.focus_module, regs[9], g_mdi.p_er_rw);
        g_mdi.helper_vs_entry_logged = 0;
        emit_helper_vs_entry();
    }

    thumb = (cpsr & (1u << 5)) != 0;
    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) return;
        half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load && rn == 9) {
            uint32_t eff = regs[9] + imm;
            uint32_t val = 0;
            const char *used = "STATE";
            const char *fclass = "UNKNOWN";
            if (regs[9] == 0) {
                g_mdi.saw_r9_zero_load = 1;
                fclass = "RW";
            } else if (g_mdi.allocated_rw_base && eff >= g_mdi.allocated_rw_base &&
                       eff < g_mdi.allocated_rw_base +
                                 (g_mdi.allocated_rw_size ? g_mdi.allocated_rw_size : 1u)) {
                fclass = "RW";
            }
            if (regs[9])
                guest_memory_uc_peek_u32((struct uc_struct *)uc, eff, &val);
            if (g_mdi.global_count < GWY_MDI_GLOBAL_MAX) {
                g_mdi.global_count++;
                if (!g_mdi.first_global_logged) {
                    g_mdi.first_global_logged = 1;
                    g_mdi.first_global_imm = imm;
                    g_mdi.first_global_value = val;
                }
                printf("[ENTRY_GLOBAL_ACCESS] pc_offset=0x%X address_expr=r9+0x%X "
                       "effective_address=0x%X value=0x%X field_class=%s used_as=%s "
                       "r9=0x%X evidence=OBSERVED\n",
                       g_mdi.code_base ? ((pc & ~1u) - g_mdi.code_base) : 0u, imm, eff, val, fclass,
                       used, regs[9]);
                fflush(stdout);
            }
        }
        /* CMP Rn,#imm after r9 load — mark branch condition use */
        if ((half & 0xF800u) == 0x2800u && g_mdi.first_global_logged) g_mdi.saw_r9_branch = 1;
    } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rn == 9) {
            uint32_t eff = regs[9] + imm;
            uint32_t val = 0;
            if (!regs[9]) g_mdi.saw_r9_zero_load = 1;
            else guest_memory_uc_peek_u32((struct uc_struct *)uc, eff, &val);
            if (!g_mdi.first_global_logged) {
                g_mdi.first_global_logged = 1;
                g_mdi.first_global_imm = imm;
                g_mdi.first_global_value = val;
            }
            if (g_mdi.global_count < GWY_MDI_GLOBAL_MAX) {
                g_mdi.global_count++;
                printf("[ENTRY_GLOBAL_ACCESS] pc_offset=0x%X address_expr=r9+0x%X "
                       "effective_address=0x%X value=0x%X field_class=%s used_as=STATE "
                       "r9=0x%X evidence=OBSERVED\n",
                       g_mdi.code_base ? ((pc & ~1u) - g_mdi.code_base) : 0u, imm, eff, val,
                       regs[9] ? "RW" : "RW", regs[9]);
                fflush(stdout);
            }
        }
    }
    (void)rt;
}

void ext_module_data_init_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                       uint32_t r9) {
    if (!ext_module_data_init_enabled()) return;
    (void)fault_pc;
    (void)fault_addr;
    (void)r0;
    g_mdi.entry_fault = 1;
    if (r9 && !g_mdi.entry_r9) {
        g_mdi.entry_r9 = r9;
        classify_r9(r9, g_mdi.entry_r9_class);
    }
    g_mdi.class_logged = 0;
    g_mdi.helper_vs_entry_logged = 0;
    g_mdi.identity_logged = 0;
    classify_and_emit();
}
