#include "gwy_launcher/robotol_idle_watch.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/platform_handler_registry.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define E8C_MAX_FLAGS 24

typedef struct E8cFlag {
    int used;
    uint32_t addr;
    uint32_t offset; /* er_rw-relative; 0 if absolute-only */
    uint32_t width;  /* 1 or 4 */
    uint32_t last_val;
    int have_last;
    uint32_t write_n;
    uint32_t last_writer_pc;
    int fe8_band; /* E8E: offset in FE0..FF0 */
} E8cFlag;

static struct {
    int enabled_known;
    int enabled;
    int armed;
    void *uc;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
    uint32_t tick;
    uint32_t er_rw_base;
    uint32_t er_rw_size;
    char owner[64];
    E8cFlag flags[E8C_MAX_FLAGS];
    int nflags;
    /* helper fx */
    int fx_active;
    uint32_t fx_r0;
    uint32_t fx_r1;
    uint32_t fx_writes;
    /* E8D */
    int e8d_diff;
    int e8d_probe;
    int probe_done;
    int have_prev_win;
    uint8_t prev_win[0x600];
    uint32_t offset_base_cached;
    /* E8E */
    int e8e_fe8_watch;
    int e8e_probe;
    int drain_order; /* 0=legacy B, 'A','B','C' */
} g_e8c;

#define E8D_WIN_OFF 0xC00u
#define E8D_WIN_LEN 0x600u

static int g_armed_base;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

static int parse_drain_order(void) {
    const char *e = getenv("JJFB_E8E_DRAIN_ORDER");
    if (!e || !e[0]) return 0;
    if ((e[0] == 'A' || e[0] == 'a') && e[1] == '\0') return 'A';
    if ((e[0] == 'B' || e[0] == 'b') && e[1] == '\0') return 'B';
    if ((e[0] == 'C' || e[0] == 'c') && e[1] == '\0') return 'C';
    return 0;
}

int robotol_idle_watch_drain_order(void) {
    (void)robotol_idle_watch_enabled();
    return g_e8c.drain_order;
}

int robotol_idle_watch_enabled(void) {
    if (!g_e8c.enabled_known) {
        g_e8c.enabled = env1("JJFB_E8C_IDLE_WATCH") || env1("JJFB_E8D_EARLY_WATCH") ||
                        env1("JJFB_E8D_ERW_DIFF") || env1("JJFB_E8D_10165_PROBE") ||
                        env1("JJFB_E8E_FE8_WATCH") || env1("JJFB_E8E_EVENT_PROBE");
        g_e8c.e8d_diff = env1("JJFB_E8D_ERW_DIFF");
        g_e8c.e8d_probe = env1("JJFB_E8D_10165_PROBE");
        g_e8c.e8e_fe8_watch = env1("JJFB_E8E_FE8_WATCH");
        g_e8c.e8e_probe = env1("JJFB_E8E_EVENT_PROBE");
        g_e8c.drain_order = parse_drain_order();
        g_e8c.enabled_known = 1;
    }
    return g_e8c.enabled;
}

void robotol_idle_watch_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_e8c.uc && g_e8c.armed && g_e8c.hook) {
        uc_hook_del((uc_engine *)g_e8c.uc, g_e8c.hook);
    }
#endif
    memset(&g_e8c, 0, sizeof(g_e8c));
    g_armed_base = 0;
}

void robotol_idle_watch_bind_uc(void *uc) {
    g_e8c.uc = uc;
}

void robotol_idle_watch_set_tick(uint32_t tick) { g_e8c.tick = tick; }

static int parse_hex_list(const char *s, uint32_t *out, int maxn) {
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p && n < maxn) {
        char *end = NULL;
        unsigned long v;
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        v = strtoul(p, &end, 0);
        if (end == p) break;
        out[n++] = (uint32_t)v;
        p = end;
    }
    return n;
}

static uint32_t peek_flag(void *uc, const E8cFlag *f) {
    uint8_t b = 0;
    uint32_t w = 0;
    if (!uc || !f->addr) return 0;
    if (f->width == 4) {
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, f->addr, &w)) return w;
        return 0;
    }
    if (guest_memory_uc_peek((struct uc_struct *)uc, f->addr, &b, 1)) return (uint32_t)(int8_t)b;
    return 0;
}

#ifdef GWY_HAVE_UNICORN
static void on_e8c_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                             int64_t value, void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t pc = 0;
    int i;
    (void)type;
    (void)size;
    (void)user_data;
    if (g_e8c.fx_active) g_e8c.fx_writes++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    for (i = 0; i < g_e8c.nflags; i++) {
        E8cFlag *f = &g_e8c.flags[i];
        uint32_t oldv, newv;
        if (!f->used) continue;
        if (addr < f->addr || addr >= f->addr + f->width) continue;
        oldv = f->have_last ? f->last_val : peek_flag(uc, f);
        /* Unicorn passes the stored value for the access; re-peek after write. */
        newv = (uint32_t)value;
        if (f->width == 1) newv = (uint32_t)(uint8_t)value;
        if (f->width == 4 && size >= 4) newv = (uint32_t)value;
        else if (f->width != 1) newv = peek_flag(uc, f);
        printf("[JJFB_E8C_FLAG_WRITE] addr=0x%X off=0x%X old=0x%X new=0x%X writer_pc=0x%X "
               "tick=%u size=%d evidence=OBSERVED\n",
               f->addr, f->offset, oldv, newv, pc, g_e8c.tick, size);
        if (f->fe8_band) {
            printf("[JJFB_E8E_FE8_WRITE] addr=0x%X off=0x%X old=0x%X new=0x%X writer_pc=0x%X "
                   "tick=%u size=%d role=queue_token_or_helper_ret evidence=TARGET_OBSERVED\n",
                   f->addr, f->offset, oldv, newv, pc, g_e8c.tick, size);
        }
        fflush(stdout);
        /* Idle-state transitions only for C44/C9D/CF5-class watches — not FE8/queue band. */
        if (f->have_last && oldv != newv && !f->fe8_band && f->offset != 0x7FCu &&
            f->offset != 0x844u && f->offset != 0xB7Du) {
            printf("[JJFB_E8C_FLAG_TRANSITION] addr=0x%X off=0x%X from=0x%X to=0x%X "
                   "writer_pc=0x%X tick=%u evidence=OBSERVED\n",
                   f->addr, f->offset, oldv, newv, pc, g_e8c.tick);
            fflush(stdout);
        }
        f->last_val = newv;
        f->have_last = 1;
        f->write_n++;
        f->last_writer_pc = pc;
    }
}
#endif

static void classify_owner(uint32_t addr) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    size_t i;
    if (!reg) {
        snprintf(g_e8c.owner, sizeof(g_e8c.owner), "?");
        return;
    }
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        if (m->data.start_of_er_rw && m->data.er_rw_size && addr >= m->data.start_of_er_rw &&
            addr < m->data.start_of_er_rw + m->data.er_rw_size) {
            snprintf(g_e8c.owner, sizeof(g_e8c.owner), "%s",
                     m->resolved_name[0] ? m->resolved_name : m->requested_name);
            g_e8c.er_rw_base = m->data.start_of_er_rw;
            g_e8c.er_rw_size = m->data.er_rw_size;
            return;
        }
        if (m->map.guest_code_base && addr >= m->map.guest_code_base &&
            addr < m->map.guest_code_base + m->map.guest_code_size) {
            snprintf(g_e8c.owner, sizeof(g_e8c.owner), "%s/code",
                     m->resolved_name[0] ? m->resolved_name : m->requested_name);
            return;
        }
    }
    snprintf(g_e8c.owner, sizeof(g_e8c.owner), "unmapped");
}

static int find_robotol_er_rw(uint32_t *base_out, uint32_t *size_out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!reg) return 0;
    m = module_registry_find(reg, "robotol.ext");
    if (!m || !m->data.start_of_er_rw) return 0;
    *base_out = m->data.start_of_er_rw;
    *size_out = m->data.er_rw_size ? m->data.er_rw_size : 0x2000u;
    return 1;
}

void robotol_idle_watch_try_arm(void *uc) {
    uint32_t offsets[E8C_MAX_FLAGS];
    uint32_t addrs[E8C_MAX_FLAGS];
    int no = 0, na = 0, i;
    uint32_t er_base = 0, er_size = 0, offset_base = 0;
    uint32_t range_lo = 0xFFFFFFFFu, range_hi = 0;
    const char *soff;
    const char *saddr;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif

    if (!robotol_idle_watch_enabled()) return;
    if (!uc) uc = g_e8c.uc;
    if (!uc) return;

    soff = getenv("JJFB_E8C_WATCH_OFFSETS");
    saddr = getenv("JJFB_E8C_WATCH_ADDRS");
    no = parse_hex_list(soff, offsets, E8C_MAX_FLAGS);
    na = parse_hex_list(saddr, addrs, E8C_MAX_FLAGS);
    if (!no && !na) {
        if (!g_e8c.armed) {
            printf("[JJFB_E8C_IDLE_WATCH] skip=no_addrs evidence=OBSERVED\n");
            fflush(stdout);
        }
        return;
    }

    if (no && !find_robotol_er_rw(&er_base, &er_size)) {
        /* Not published yet — retry later. */
        return;
    }
    offset_base = er_base;
    /* Idle loop uses ADD Rd,R9 — prefer live R9 when it sits in the ER_RW band. */
    if (no && uc) {
        uint32_t r9 = 0;
        if (guest_memory_uc_read_r9((struct uc_struct *)uc, &r9) && r9) {
            if (er_base && er_size && r9 >= er_base && r9 < er_base + er_size)
                offset_base = r9;
            else if (er_base && r9 + 0x20u >= er_base && r9 <= er_base + 0x20u)
                offset_base = r9;
        }
    }

    if (g_e8c.armed && g_armed_base == (int)offset_base) return;

#ifdef GWY_HAVE_UNICORN
    if (g_e8c.armed && g_e8c.hook) {
        uc_hook_del((uc_engine *)uc, g_e8c.hook);
        g_e8c.hook = 0;
        g_e8c.armed = 0;
        printf("[JJFB_E8C_IDLE_WATCH] rearm old_base=0x%X new_base=0x%X evidence=OBSERVED\n",
               (unsigned)g_armed_base, offset_base);
        fflush(stdout);
    }
#endif

    g_e8c.nflags = 0;
    memset(g_e8c.flags, 0, sizeof(g_e8c.flags));
    for (i = 0; i < no && g_e8c.nflags < E8C_MAX_FLAGS; i++) {
        E8cFlag *f = &g_e8c.flags[g_e8c.nflags++];
        f->used = 1;
        f->offset = offsets[i];
        f->addr = offset_base + offsets[i];
        f->width = 1u;
        if (offsets[i] == 0x11B0u) f->width = 4u;
        if (f->addr < range_lo) range_lo = f->addr;
        if (f->addr + f->width > range_hi) range_hi = f->addr + f->width;
    }
    for (i = 0; i < na && g_e8c.nflags < E8C_MAX_FLAGS; i++) {
        E8cFlag *f = &g_e8c.flags[g_e8c.nflags++];
        f->used = 1;
        f->offset = 0;
        f->addr = addrs[i];
        f->width = 1;
        if (f->addr < range_lo) range_lo = f->addr;
        if (f->addr + f->width > range_hi) range_hi = f->addr + f->width;
    }
    /* E8E: FE8±8 word band + queue/status fields from 0x30D24C disasm. */
    if (g_e8c.e8e_fe8_watch && offset_base && g_e8c.nflags < E8C_MAX_FLAGS) {
        static const uint32_t e8e_offs[] = {0xFE0u, 0xFE4u, 0xFE8u, 0xFECu, 0xFF0u,
                                            0xB7Du, 0x7FCu, 0x844u};
        static const uint32_t e8e_widths[] = {4, 4, 4, 4, 4, 1, 4, 4};
        int j;
        for (j = 0; j < (int)(sizeof(e8e_offs) / sizeof(e8e_offs[0])) && g_e8c.nflags < E8C_MAX_FLAGS;
             j++) {
            E8cFlag *f;
            int dup = 0;
            for (i = 0; i < g_e8c.nflags; i++) {
                if (g_e8c.flags[i].offset == e8e_offs[j] && g_e8c.flags[i].addr)
                    dup = 1;
            }
            if (dup) continue;
            f = &g_e8c.flags[g_e8c.nflags++];
            f->used = 1;
            f->offset = e8e_offs[j];
            f->addr = offset_base + e8e_offs[j];
            f->width = e8e_widths[j];
            f->fe8_band = (e8e_offs[j] >= 0xFE0u && e8e_offs[j] <= 0xFF0u) ? 1 : 0;
            if (f->addr < range_lo) range_lo = f->addr;
            if (f->addr + f->width > range_hi) range_hi = f->addr + f->width;
        }
        printf("[JJFB_E8E_FE8_WATCH] armed_band=0xFE0..0xFF0 plus=B7D,7FC,844 offset_base=0x%X "
               "note=FE8=helper_0x3046A8_ret_slot evidence=TARGET_OBSERVED\n",
               offset_base);
        fflush(stdout);
    }

    if (g_e8c.nflags == 0 || range_hi <= range_lo) return;

    g_e8c.er_rw_base = er_base;
    g_e8c.er_rw_size = er_size;
    g_armed_base = (int)offset_base;
    classify_owner(g_e8c.flags[0].addr);

#ifdef GWY_HAVE_UNICORN
    ue = uc_hook_add((uc_engine *)uc, &g_e8c.hook, UC_HOOK_MEM_WRITE, (void *)on_e8c_mem_write, NULL,
                     (uint64_t)range_lo, (uint64_t)(range_hi - 1));
    if (ue != UC_ERR_OK) {
        printf("[JJFB_E8C_IDLE_WATCH] arm_failed uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        fflush(stdout);
        return;
    }
#endif
    g_e8c.armed = 1;
    g_e8c.uc = uc;
    printf("[JJFB_E8C_IDLE_WATCH] armed=1 er_rw=0x%X offset_base=0x%X size=0x%X owner=%s "
           "nflags=%d range=0x%X..0x%X evidence=OBSERVED\n",
           er_base, offset_base, er_size, g_e8c.owner, g_e8c.nflags, range_lo, range_hi - 1);
    for (i = 0; i < g_e8c.nflags; i++) {
        E8cFlag *f = &g_e8c.flags[i];
        uint32_t v = peek_flag(uc, f);
        f->last_val = v;
        f->have_last = 1;
        printf("[JJFB_E8C_FLAG_ARM] addr=0x%X off=0x%X width=%u init=0x%X owner=%s "
               "evidence=OBSERVED\n",
               f->addr, f->offset, f->width, v, g_e8c.owner);
    }
    fflush(stdout);
}

void robotol_idle_watch_snap(void *uc, const char *reason) {
    int i;
    if (!robotol_idle_watch_enabled()) return;
    if (!uc) uc = g_e8c.uc;
    robotol_idle_watch_try_arm(uc);
    if (!g_e8c.armed || !uc) return;
    for (i = 0; i < g_e8c.nflags; i++) {
        E8cFlag *f = &g_e8c.flags[i];
        uint32_t v;
        if (!f->used) continue;
        v = peek_flag(uc, f);
        printf("[JJFB_E8C_FLAG_SNAP] tick=%u addr=0x%X off=0x%X val=0x%X writes=%u "
               "last_writer=0x%X reason=%s evidence=OBSERVED\n",
               g_e8c.tick, f->addr, f->offset, v, f->write_n, f->last_writer_pc,
               reason ? reason : "-");
        if (f->have_last && f->last_val != v) {
            printf("[JJFB_E8C_FLAG_TRANSITION] addr=0x%X off=0x%X from=0x%X to=0x%X "
                   "writer_pc=0x%X tick=%u via=snap evidence=OBSERVED\n",
                   f->addr, f->offset, f->last_val, v, f->last_writer_pc, g_e8c.tick);
        }
        f->last_val = v;
        f->have_last = 1;
    }
    fflush(stdout);
}

void robotol_idle_watch_helper_fx_begin(uint32_t r0, uint32_t r1) {
    int watch = 0;
    if (!robotol_idle_watch_enabled()) return;
    if (r0 == 0x1E209u && r1 == 0x9u) watch = 1;
    if (r0 == 0x1E209u && (r1 == 0u || r1 == 0x18u || r1 == 0x0Bu)) watch = 1;
    if (!watch) return;
    g_e8c.fx_active = 1;
    g_e8c.fx_r0 = r0;
    g_e8c.fx_r1 = r1;
    g_e8c.fx_writes = 0;
}

void robotol_idle_watch_helper_fx_end(uint32_t r0, uint32_t r1, uint32_t ret) {
    if (!g_e8c.fx_active) return;
    printf("[JJFB_E8C_HELPER_FX] r0=0x%X r1=0x%X ret=0x%X watched_writes=%u tick=%u "
           "note=observe_only_no_ret_mutation evidence=OBSERVED\n",
           r0, r1, ret, g_e8c.fx_writes, g_e8c.tick);
    fflush(stdout);
    g_e8c.fx_active = 0;
}

void robotol_idle_watch_on_handler_register(uint32_t plat_code, uint32_t family, uint32_t handler) {
    const char *role = "?";
    const char *drain = "no";
    const char *tramp = "-";
    if (!robotol_idle_watch_enabled()) return;
    if (plat_code == 0x10140u) {
        role = "lifecycle_period";
        drain = "yes_lifecycle";
    } else if (plat_code == 0x10165u) {
        role = "enqueue_event";
        drain = "probe_or_drain_order_only";
        tramp = "BL_0x30D24C";
    } else if (plat_code == 0x10162u) {
        role = "alloc_or_enqueue";
    } else if (plat_code == 0x10102u) {
        role = "family_register";
    }
    printf("[JJFB_E8D_HANDLER_MAP] code=0x%X family=0x%X handler=0x%X role=%s "
           "evidence=CROSS_TARGET+docs/06\n",
           plat_code, family, handler, role);
    printf("[JJFB_E8E_HANDLER_MAP] code=0x%X family=0x%X handler=0x%X role=%s "
           "trampoline_target=%s host_drain=%s evidence=CROSS_TARGET+TARGET_OBSERVED\n",
           plat_code, family, handler, role, tramp, drain);
    fflush(stdout);
}

static uint32_t current_offset_base(void *uc) {
    uint32_t er_base = 0, er_size = 0, base = 0, r9 = 0;
    if (!find_robotol_er_rw(&er_base, &er_size)) return g_armed_base ? (uint32_t)g_armed_base : 0;
    base = er_base;
    if (uc && guest_memory_uc_read_r9((struct uc_struct *)uc, &r9) && r9) {
        if (r9 >= er_base && r9 < er_base + er_size) base = r9;
        else if (r9 + 0x20u >= er_base && r9 <= er_base + 0x20u) base = r9;
    }
    return base;
}

void robotol_idle_watch_note_stage(void *uc, const char *stage) {
    uint32_t base;
    uint8_t win[E8D_WIN_LEN];
    uint32_t i, changed = 0, first = 0;
    int have_first = 0;
    if (!robotol_idle_watch_enabled()) return;
    if (!uc) uc = g_e8c.uc;
    if (env1("JJFB_E8D_EARLY_WATCH") || env1("JJFB_E8C_IDLE_WATCH"))
        robotol_idle_watch_try_arm(uc);
    robotol_idle_watch_snap(uc, stage ? stage : "stage");
    if (!g_e8c.e8d_diff || !uc) return;
    base = current_offset_base(uc);
    if (!base) return;
    g_e8c.offset_base_cached = base;
    memset(win, 0, sizeof(win));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, base + E8D_WIN_OFF, win, E8D_WIN_LEN)) {
        printf("[JJFB_E8D_ERW_DIFF] stage=%s peek_fail base=0x%X evidence=OBSERVED\n",
               stage ? stage : "?", base);
        fflush(stdout);
        return;
    }
    if (g_e8c.have_prev_win) {
        for (i = 0; i < E8D_WIN_LEN; i++) {
            if (win[i] != g_e8c.prev_win[i]) {
                changed++;
                if (!have_first) {
                    first = i;
                    have_first = 1;
                }
            }
        }
    }
    printf("[JJFB_E8D_ERW_DIFF] stage=%s base=0x%X win=0x%X..0x%X changed=%u first_off=0x%X "
           "tick=%u evidence=OBSERVED\n",
           stage ? stage : "?", base, E8D_WIN_OFF, E8D_WIN_OFF + E8D_WIN_LEN - 1, changed,
           have_first ? (E8D_WIN_OFF + first) : 0u, g_e8c.tick);
    /* Compact hex of flag bytes of interest */
    {
        static const uint32_t offs[] = {0xC44u, 0xC9Du, 0xCD1u, 0xCF5u, 0x11B0u};
        int k;
        printf("[JJFB_E8D_ERW_FLAGS] stage=%s", stage ? stage : "?");
        for (k = 0; k < 5; k++) {
            uint32_t o = offs[k];
            uint32_t idx = o - E8D_WIN_OFF;
            uint32_t v = 0;
            if (o >= E8D_WIN_OFF && idx < E8D_WIN_LEN) {
                if (o == 0x11B0u && idx + 3 < E8D_WIN_LEN)
                    v = (uint32_t)win[idx] | ((uint32_t)win[idx + 1] << 8) |
                        ((uint32_t)win[idx + 2] << 16) | ((uint32_t)win[idx + 3] << 24);
                else
                    v = win[idx];
            }
            printf(" 0x%X=0x%X", o, v);
        }
        printf(" evidence=OBSERVED\n");
    }
    memcpy(g_e8c.prev_win, win, E8D_WIN_LEN);
    g_e8c.have_prev_win = 1;
    fflush(stdout);
}

void robotol_idle_watch_try_10165_probe(void *uc) {
    uint32_t handler, family;
    uint32_t stop = 0x80000u; /* CODE_ADDRESS-class stop used by lifecycle */
    uint32_t r9_save = 0, r9_run = 0;
    uint32_t r0_arg = 0, r1_arg = 0, r2_arg = 0, r3_arg = 0;
    uint32_t fe8_before = 0, fe8_after = 0, b7d_before = 0, b7d_after = 0;
    uint32_t c44 = 0, c9d = 0, cf5 = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    const char *cand;
    ModuleRegistry *reg;
    const GwyLoadedModule *owner;
    int ok;
    int want = 0;

    if (!robotol_idle_watch_enabled() || g_e8c.probe_done) return;
    want = g_e8c.e8d_probe || g_e8c.e8e_probe;
    if (!want) return;
    if (!uc) uc = g_e8c.uc;
    if (!uc) return;
    handler = platform_handler_registry_get(0x10165u);
    if (!handler) return;
    family = platform_handler_registry_family(0x10165u);
    cand = getenv("JJFB_E8E_CANDIDATE");
    if (!cand || !cand[0]) cand = getenv("JJFB_E8D_10165_CANDIDATE");
    if (!cand || !cand[0]) cand = "ZERO_ARGS";

    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    reg = gwy_ext_loader_bound_registry();
    owner = reg ? module_registry_find_by_code_addr(reg, handler & ~1u) : NULL;
    r9_run = r9_save;
    if (owner && owner->data.start_of_er_rw) r9_run = owner->data.start_of_er_rw;
    {
        uint32_t er_base = 0, er_size = 0;
        if (find_robotol_er_rw(&er_base, &er_size) && r9_save >= er_base &&
            r9_save < er_base + er_size)
            r9_run = r9_save;
    }
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);

    /* ABI-gated candidates from out/e8e_tmp/abi_inference — no spray. */
    if (strcmp(cand, "ZERO_ARGS") == 0 || strcmp(cand, "HELPER_ONLY_PASSTHROUGH") == 0) {
        r0_arg = 0;
        r1_arg = 0;
    } else if (strcmp(cand, "R0_NULL_R1_CTX") == 0 || strcmp(cand, "R0_NULL_R1_R9") == 0) {
        r0_arg = 0;
        r1_arg = r9_run;
    } else if (strcmp(cand, "R0_EVENTCODE_1") == 0) {
        r0_arg = 1;
        r1_arg = 0;
    } else if (strcmp(cand, "R0_EVENTCODE_2") == 0) {
        /* Long path uses MOVS r3,#2 when forwarding to plat 0x101AB. */
        r0_arg = 2;
        r1_arg = 0;
    } else if (strcmp(cand, "R0_EVENTCODE_3") == 0) {
        r0_arg = 3;
        r1_arg = 0;
    } else if (strcmp(cand, "R0_EVENT_PTR_MIN") == 0) {
        /* Minimal guest block at ER_RW scratch; never writes C44/C9D/CF5. */
        uint32_t scratch = r9_run ? (r9_run + 0x1E00u) : 0;
        uint8_t blk[16];
        memset(blk, 0, sizeof(blk));
        blk[0] = 2; /* event-code-like first byte; HYPOTHESIS layout */
        if (scratch && guest_memory_uc_poke((struct uc_struct *)uc, scratch, blk, sizeof(blk))) {
            r0_arg = scratch;
            r1_arg = 0;
            printf("[JJFB_E8E_EVENT_PROBE] scratch=0x%X size=16 note=observe_only evidence=HYPOTHESIS\n",
                   scratch);
        } else {
            printf("[JJFB_E8E_EVENT_PROBE] candidate=%s skip=scratch_poke_failed evidence=OBSERVED\n",
                   cand);
            fflush(stdout);
            return;
        }
    } else {
        printf("[JJFB_E8E_EVENT_PROBE] candidate=%s skip=unknown_hold_for_abi evidence=OBSERVED\n",
               cand);
        fflush(stdout);
        return;
    }

    if (r9_run) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + 0xFE8u, &fe8_before);
        {
            uint8_t b = 0;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xB7Du, &b, 1))
                b7d_before = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &b, 1)) c44 = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC9Du, &b, 1)) c9d = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCF5u, &b, 1)) cf5 = b;
        }
    }

    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = r0_arg;
    abi.set_r1 = 1;
    abi.r1 = r1_arg;
    abi.set_r2 = 1;
    abi.r2 = r2_arg;
    abi.set_r3 = 1;
    abi.r3 = r3_arg;
    abi.set_lr = 1;
    abi.lr = stop;
    printf("[JJFB_E8D_10165_FIRE] candidate=%s handler=0x%X family=0x%X r9=0x%X "
           "r0=0x%X r1=0x%X note=observe_only evidence=HYPOTHESIS+TARGET_OBSERVED\n",
           cand, handler, family, r9_run, r0_arg, r1_arg);
    printf("[JJFB_E8E_EVENT_PROBE] candidate=%s handler=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "fe8_before=0x%X b7d_before=0x%X c44=0x%X c9d=0x%X cf5=0x%X "
           "evidence=TARGET_OBSERVED\n",
           cand, handler, r0_arg, r1_arg, r2_arg, r3_arg, fe8_before, b7d_before, c44, c9d, cf5);
    fflush(stdout);
    g_e8c.probe_done = 1;
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, handler, stop, 200000ull, &abi, &out);
    if (r9_run) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + 0xFE8u, &fe8_after);
        {
            uint8_t b = 0;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xB7Du, &b, 1))
                b7d_after = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &b, 1)) c44 = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC9Du, &b, 1)) c9d = b;
            if (guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCF5u, &b, 1)) cf5 = b;
        }
    }
    printf("[JJFB_E8D_10165_FIRE_DONE] candidate=%s ok=%d end=%s pc_after=0x%X r0_after=0x%X "
           "evidence=OBSERVED\n",
           cand, ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, out.r0_after);
    printf("[JJFB_E8E_EVENT_PROBE_DONE] candidate=%s ok=%d fe8_after=0x%X b7d_after=0x%X "
           "c44=0x%X c9d=0x%X cf5=0x%X fe8_changed=%d flags_still_zero=%d evidence=OBSERVED\n",
           cand, ok, fe8_after, b7d_after, c44, c9d, cf5, fe8_before != fe8_after,
           (c44 == 0 && c9d == 0 && cf5 == 0));
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
    robotol_idle_watch_note_stage(uc, "after_10165_probe");
}

