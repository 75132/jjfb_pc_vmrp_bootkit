#include "gwy_launcher/ext_mrpgcmap_entry_order.h"
#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct {
    char name[48];
    uint32_t base;
    uint32_t size;
    uint32_t intended; /* base+8 DOCUMENTED */
    uint32_t observed_first;
    int mapped;
    int entry_ran;
    int entry_hit;
    int entry_returned;
    int cont_seen;
    int audit_emitted;
} EntryMod;

static struct {
    int mode_known;
    GwyMrpgcmapEntryMode mode;
    void *uc;
    int finalized;
    int in_entry_emu;
    int any_entry_hit;
    int any_entry_ran;
    uint32_t last_fault_pc;
    uint32_t last_fault_addr;
    EntryMod mods[8];
    int mod_count;
} g_eo;

static int name_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int is_shell_ext(const char *s) {
    return name_has(s, "gbrwcore") || name_has(s, "gamelist") || name_has(s, "gbrwshell");
}

static int mode_applies(const char *module_name) {
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_OFF) return 0;
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_OBSERVE) return is_shell_ext(module_name);
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY) return name_has(module_name, "gbrwcore");
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_SHELL) return is_shell_ext(module_name);
    return 0;
}

static int mode_may_run_entry(const char *module_name) {
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY) return name_has(module_name, "gbrwcore");
    if (g_eo.mode == GWY_MRPGCMAP_ENTRY_SHELL) return is_shell_ext(module_name);
    return 0;
}

GwyMrpgcmapEntryMode ext_mrpgcmap_entry_order_mode(void) {
    const char *e;
    if (g_eo.mode_known) return g_eo.mode;
    e = getenv("JJFB_FIX_MRPGCMAP_ENTRY_ORDER");
    g_eo.mode = GWY_MRPGCMAP_ENTRY_OFF;
    if (e && e[0]) {
        if (strcmp(e, "observe") == 0) g_eo.mode = GWY_MRPGCMAP_ENTRY_OBSERVE;
        else if (strcmp(e, "gbrwcore_only") == 0) g_eo.mode = GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY;
        else if (strcmp(e, "shell") == 0 || strcmp(e, "shell_core") == 0)
            g_eo.mode = GWY_MRPGCMAP_ENTRY_SHELL;
        else if (e[0] == '1') g_eo.mode = GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY;
    }
    g_eo.mode_known = 1;
    return g_eo.mode;
}

int ext_mrpgcmap_entry_order_enabled(void) {
    return ext_mrpgcmap_entry_order_mode() != GWY_MRPGCMAP_ENTRY_OFF;
}

void ext_mrpgcmap_entry_order_reset(void) {
    memset(&g_eo, 0, sizeof(g_eo));
}

void ext_mrpgcmap_entry_order_bind_uc(void *uc) { g_eo.uc = uc; }

int ext_mrpgcmap_entry_order_entry_hit(void) { return g_eo.any_entry_hit; }
int ext_mrpgcmap_entry_order_entry_ran(void) { return g_eo.any_entry_ran; }

static EntryMod *find_mod(const char *name) {
    int i;
    if (!name) return NULL;
    for (i = 0; i < g_eo.mod_count; i++) {
        if (strstr(g_eo.mods[i].name, name) || strstr(name, g_eo.mods[i].name))
            return &g_eo.mods[i];
    }
    return NULL;
}

static EntryMod *find_or_add(const char *name, uint32_t base, uint32_t size) {
    EntryMod *m;
    const char *label;
    if (!name || !base) return NULL;
    m = find_mod(name);
    if (m) {
        if (base) m->base = base;
        if (size) m->size = size;
        if (m->base) m->intended = m->base + 8u;
        return m;
    }
    if (g_eo.mod_count >= 8) return NULL;
    m = &g_eo.mods[g_eo.mod_count++];
    memset(m, 0, sizeof(*m));
    if (name_has(name, "gbrwcore")) label = "gbrwcore.ext";
    else if (name_has(name, "gamelist")) label = "gamelist.ext";
    else if (name_has(name, "gbrwshell")) label = "gbrwshell.ext";
    else label = name;
    snprintf(m->name, sizeof(m->name), "%s", label);
    m->base = base;
    m->size = size;
    m->intended = base ? (base + 8u) : 0;
    return m;
}

static void emit_order(EntryMod *m, const char *state) {
    if (!m || !state) return;
    printf("[JJFB_ENTRY_ORDER] module=%s state=%s base=0x%X intended=0x%X observed_first=0x%X "
           "evidence=TARGET_OBSERVED\n",
           m->name, state, m->base, m->intended, m->observed_first);
    fflush(stdout);
}

static void emit_audit(EntryMod *m) {
    if (!m || m->audit_emitted) return;
    if (!m->observed_first) return;
    m->audit_emitted = 1;
    printf("[JJFB_6K_ENTRY_AUDIT] module=%s image_base=0x%X intended_entry=0x%X "
           "observed_first_pc=0x%X match=%s evidence=TARGET_OBSERVED\n",
           m->name, m->base, m->intended, m->observed_first,
           (m->observed_first && m->intended &&
            gwy_guest_pc_norm(m->observed_first) == gwy_guest_pc_norm(m->intended))
               ? "yes"
               : "no");
    fflush(stdout);
}

void ext_mrpgcmap_entry_order_on_module_mapped(const char *module_name, uint32_t code_base,
                                               uint32_t code_size) {
    EntryMod *m;
    if (!ext_mrpgcmap_entry_order_enabled() || !mode_applies(module_name)) return;
    m = find_or_add(module_name, code_base, code_size);
    if (!m) return;
    m->mapped = 1;
    emit_order(m, "loaded");
}

void ext_mrpgcmap_entry_order_on_module_registered(const char *module_name, uint32_t code_base,
                                                   uint32_t helper) {
    EntryMod *m;
    (void)helper;
    if (!ext_mrpgcmap_entry_order_enabled() || !mode_applies(module_name)) return;
    m = find_or_add(module_name, code_base, 0);
    if (!m) return;
    emit_order(m, "registered");
}

void ext_mrpgcmap_entry_order_on_first_pc(const char *module_name, uint32_t code_base,
                                          uint32_t observed_pc) {
    EntryMod *m;
    if (!ext_mrpgcmap_entry_order_enabled() || !mode_applies(module_name)) return;
    m = find_or_add(module_name, code_base, 0);
    if (!m) return;
    if (!m->observed_first) m->observed_first = observed_pc;
    emit_audit(m);
}

void ext_mrpgcmap_entry_order_on_code(void *uc, const char *module_name, uint32_t pc) {
    EntryMod *m;
    uint32_t pn;
    (void)uc;
    if (!ext_mrpgcmap_entry_order_enabled() || !g_eo.in_entry_emu) return;
    if (!mode_applies(module_name)) return;
    m = find_mod(module_name);
    if (!m || !m->intended) return;
    pn = gwy_guest_pc_norm(pc);
    if (pn == gwy_guest_pc_norm(m->intended) ||
        (pn >= gwy_guest_pc_norm(m->intended) && pn < gwy_guest_pc_norm(m->intended) + 0x20u)) {
        if (!m->entry_hit) {
            m->entry_hit = 1;
            g_eo.any_entry_hit = 1;
            printf("[JJFB_MRPGCMAP_ENTRY_HIT] module=%s pc=0x%X expected=0x%X "
                   "evidence=TARGET_OBSERVED\n",
                   m->name, pc, m->intended);
            fflush(stdout);
        }
    }
}

#ifdef GWY_HAVE_UNICORN
static int peek_is_thumb_likely(void *uc, uint32_t addr) {
    uint32_t word = 0;
    uint16_t h;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~1u, &word)) return 1;
    h = (uint16_t)(word & 0xFFFFu);
    if ((h & 0xFF00) == 0xB500) return 1;
    return 0;
}

static int run_documented_entry(void *uc, EntryMod *m) {
    uint32_t entry;
    uint32_t stop;
    uint32_t start;
    int thumb;
    char err[96];
    int ok;

    if (!uc || !m || !m->base || m->entry_ran) return 0;
    entry = m->base + 8u; /* DOCUMENTED MRPGCMAP image+8; never use registry header symbol */
    m->intended = entry;
    stop = m->base;
    thumb = peek_is_thumb_likely(uc, entry);
    start = thumb ? (entry | 1u) : entry;

    printf("[JJFB_MRPGCMAP_ENTRY] module=%s base=0x%X raw_entry=0x%X thumb_entry=0x%X "
           "norm=0x%X thumb=%s r0=1 stop=0x%X evidence=DOCUMENTED\n",
           m->name, m->base, entry, entry | 1u, gwy_guest_pc_norm(entry),
           thumb ? "yes" : "no", stop);
    fflush(stdout);

    emit_order(m, "entry_called");
    m->entry_ran = 1;
    g_eo.any_entry_ran = 1;
    g_eo.in_entry_emu = 1;
    ext_entry_abi_cluster_audit_set_in_entry(1);
    ext_entry_abi_cluster_audit_capture_callback_regs(uc);
    err[0] = 0;
    if (ext_entry_abi_cluster_audit_enabled()) {
        ok = ext_entry_abi_cluster_audit_run_documented(uc, m->name, m->base, m->size, entry, stop,
                                                        start);
        if (!ok) {
            const char *er = ext_entry_abi_cluster_audit_last_end_reason();
            snprintf(err, sizeof(err), "%s", er ? er : "entry_abi_fail");
        }
    } else {
        ok = guest_memory_uc_run_bounded((struct uc_struct *)uc, start, stop, 1u, 200000ull, err,
                                         sizeof(err));
    }
    ext_entry_abi_cluster_audit_set_in_entry(0);
    g_eo.in_entry_emu = 0;

    if (!ok) {
        printf("[JJFB_MRPGCMAP_ENTRY] module=%s result=EMU_ERR detail=%s "
               "evidence=TARGET_OBSERVED\n",
               m->name, err[0] ? err : "?");
        fflush(stdout);
    } else {
        printf("[JJFB_MRPGCMAP_ENTRY] module=%s result=EMU_OK evidence=TARGET_OBSERVED\n",
               m->name);
        fflush(stdout);
    }

    m->entry_returned = 1;
    emit_order(m, "entry_returned");
    return 1;
}
#else
static int run_documented_entry(void *uc, EntryMod *m) {
    (void)uc;
    (void)m;
    return 0;
}
#endif

void ext_mrpgcmap_entry_order_before_continuation(void *uc, const char *module_name,
                                                  uint32_t continuation_pc) {
    EntryMod *m;
    ModuleRegistry *reg;
    const GwyLoadedModule *gm;
    uint32_t base = 0;
    uint32_t size = 0;

    if (!ext_mrpgcmap_entry_order_enabled()) return;
    if (!module_name || !mode_applies(module_name)) return;
    if (uc || g_eo.uc) ext_entry_abi_cluster_audit_capture_callback_regs(uc ? uc : g_eo.uc);

    reg = gwy_ext_loader_bound_registry();
    if (reg) {
        gm = continuation_pc ? module_registry_find_by_code_addr(reg, continuation_pc) : NULL;
        /* Shell packages: resolve by name when continuation_pc is not yet mapped. */
        if (!gm && (name_has(module_name, "gbrwcore") || name_has(module_name, "gamelist") ||
                    name_has(module_name, "gbrwshell"))) {
            size_t i;
            for (i = 0; i < reg->count; i++) {
                if (name_has(reg->modules[i].requested_name, module_name) ||
                    name_has(reg->modules[i].resolved_name, module_name) ||
                    (name_has(module_name, "gbrwcore") &&
                     (name_has(reg->modules[i].requested_name, "gbrwcore") ||
                      name_has(reg->modules[i].resolved_name, "gbrwcore"))) ||
                    (name_has(module_name, "gamelist") &&
                     (name_has(reg->modules[i].requested_name, "gamelist") ||
                      name_has(reg->modules[i].resolved_name, "gamelist"))) ||
                    (name_has(module_name, "gbrwshell") &&
                     (name_has(reg->modules[i].requested_name, "gbrwshell") ||
                      name_has(reg->modules[i].resolved_name, "gbrwshell")))) {
                    gm = &reg->modules[i];
                    break;
                }
            }
        }
        if (gm) {
            base = gm->map.guest_code_base;
            size = gm->map.guest_code_size;
            if (!module_name[0])
                module_name =
                    gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
        }
    }

    m = find_or_add(module_name, base, size);
    if (!m) return;

    if (!m->base && base) {
        m->base = base;
        m->size = size;
        m->intended = base + 8u;
    }

    if (mode_may_run_entry(m->name) && !m->entry_ran) {
        if (!uc) uc = g_eo.uc;
        /*
         * E10A-3.1b: gamelist documented entry under shell continue fills the shared P
         * with gbrwcore's ERW (or exits via br_exit) before TIMER_ARM — even after host
         * isolates a fresh ERW. Keep skip; cfg gate must be pursued without entry emu.
         */
        if (name_has(m->name, "gamelist")) {
            printf("[JJFB_MRPGCMAP_ENTRY] module=%s result=SKIP "
                   "reason=e10a31b_skip_entry_avoid_early_exit evidence=OBSERVED\n",
                   m->name);
            fflush(stdout);
            m->entry_ran = 1;
        } else if (!m->base) {
            printf("[JJFB_MRPGCMAP_ENTRY] module=%s result=SKIP reason=no_code_base "
                   "evidence=TARGET_OBSERVED\n",
                   m->name);
            fflush(stdout);
        } else {
            (void)run_documented_entry(uc, m);
        }
    }

    if (!m->cont_seen) {
        m->cont_seen = 1;
        emit_order(m, "callback_continuation");
    }
}

void ext_mrpgcmap_entry_order_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr) {
    if (!ext_mrpgcmap_entry_order_enabled()) return;
    g_eo.last_fault_pc = fault_pc;
    g_eo.last_fault_addr = fault_addr;
    if (g_eo.in_entry_emu) {
        printf("[JJFB_MRPGCMAP_ENTRY] fault_during_entry pc=0x%X addr=0x%X "
               "class=NEW_ENTRY_FAULT evidence=TARGET_OBSERVED\n",
               fault_pc, fault_addr);
        fflush(stdout);
    }
}

void ext_mrpgcmap_entry_order_finalize(const char *stop_reason) {
    int i;
    if (!ext_mrpgcmap_entry_order_enabled() || g_eo.finalized) return;
    g_eo.finalized = 1;
    printf("[JJFB_6K_ENTRY_SUMMARY] mode=%d entry_ran=%s entry_hit=%s fault_pc=0x%X "
           "fault_addr=0x%X stop=%s evidence=TARGET_OBSERVED\n",
           (int)g_eo.mode, g_eo.any_entry_ran ? "yes" : "no", g_eo.any_entry_hit ? "yes" : "no",
           g_eo.last_fault_pc, g_eo.last_fault_addr, stop_reason ? stop_reason : "?");
    for (i = 0; i < g_eo.mod_count; i++) {
        EntryMod *m = &g_eo.mods[i];
        printf("[JJFB_6K_ENTRY_SUMMARY] module=%s intended=0x%X observed=0x%X ran=%s hit=%s "
               "cont=%s\n",
               m->name, m->intended, m->observed_first, m->entry_ran ? "yes" : "no",
               m->entry_hit ? "yes" : "no", m->cont_seen ? "yes" : "no");
    }
    fflush(stdout);
}
