#include "gwy_launcher/ext_bootstrap_abi.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BS_RING 96
#define BS_R9_USE_MAX 32

typedef struct {
    uint32_t pc;
    uint32_t regs[16];
    uint32_t cpsr;
    uint64_t module_id;
} BsRingSlot;

typedef struct {
    uint32_t pc;
    uint32_t off;
    char access_kind[8];
    uint32_t effective_address;
    char address_class[16];
    char semantic[16];
} BsR9Use;

typedef struct {
    void *uc;
    int active;
    int fixr9_logged;
    int proto_logged;
    int static_logged;
    int identity_logged;
    int path_logged;
    int class_logged;
    int fault_logged;

    uint64_t focus_mid;
    char focus_module[72];
    uint32_t code_base;
    uint32_t code_size;
    uint32_t entry_pc;
    uint32_t entry_off;
    uint32_t entry_r0;
    uint32_t entry_r1;
    uint32_t entry_r2;
    uint32_t entry_r3;
    uint32_t entry_r9;
    uint32_t entry_sp;
    uint32_t entry_lr;
    uint32_t caller_r9;
    uint32_t header_entry;
    uint32_t first_observed_pc;

    int cfn_seen;
    int load_reached;
    int r9_use_count;
    int r9_used;
    BsR9Use r9_uses[BS_R9_USE_MAX];

    BsRingSlot ring[BS_RING];
    int ring_count;
    int ring_head;

    BootstrapR9Causality r9_causality;
    BootstrapAbiClass last_class;
    char next_fix[48];
} BootstrapAbiState;

static BootstrapAbiState g_bs;

void ext_bootstrap_abi_reset(void) { memset(&g_bs, 0, sizeof(g_bs)); }

int ext_bootstrap_abi_enabled(void) {
    const char *env = getenv("GWY_BOOTSTRAP_ABI");
    return env && env[0] == '1';
}

void ext_bootstrap_abi_bind_uc(void *uc) { g_bs.uc = uc; }

const char *ext_bootstrap_abi_class_name(BootstrapAbiClass c) {
    switch (c) {
        case BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED: return "BOOTSTRAP_PRESERVE_CALLER_R9_DOCUMENTED";
        case BS_ABI_PRESERVE_CALLER_R9_CROSS_TARGET: return "BOOTSTRAP_PRESERVE_CALLER_R9_CROSS_TARGET";
        case BS_ABI_TEMP_R9_REQUIRED: return "BOOTSTRAP_TEMP_R9_REQUIRED";
        case BS_ABI_R9_UNUSED: return "BOOTSTRAP_R9_UNUSED";
        case BS_ABI_ENTRY_POINTER_MISINTERPRETED: return "BOOTSTRAP_ENTRY_POINTER_MISINTERPRETED";
        case BS_ABI_RELOCATION_INCOMPLETE: return "BOOTSTRAP_RELOCATION_INCOMPLETE";
        case BS_ABI_ARGUMENT_ABI_MISSING: return "BOOTSTRAP_ARGUMENT_ABI_MISSING";
        case BS_ABI_LIFECYCLE_MISSING: return "BOOTSTRAP_LIFECYCLE_MISSING";
        default: return "UNKNOWN";
    }
}

BootstrapAbiClass ext_bootstrap_abi_last_class(void) { return g_bs.last_class; }

static const char *causality_name(BootstrapR9Causality c) {
    switch (c) {
        case BS_R9_DIRECTLY_CAUSAL: return "R9_DIRECTLY_CAUSAL";
        case BS_R9_INDIRECTLY_CAUSAL: return "R9_INDIRECTLY_CAUSAL";
        case BS_R9_USED_BUT_NOT_CAUSAL: return "R9_USED_BUT_NOT_CAUSAL";
        case BS_R9_NOT_USED_BEFORE_FAULT: return "R9_NOT_USED_BEFORE_FAULT";
        default: return "UNKNOWN";
    }
}

static void mod_name(const GwyLoadedModule *m, char *out, size_t n) {
    const char *s;
    if (!out || !n) return;
    out[0] = 0;
    if (!m) return;
    s = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    if (s && s[0]) snprintf(out, n, "%s", s);
}

static int name_is_dsm(const GwyLoadedModule *m) {
    if (!m) return 0;
    if (m->origin == MODULE_ORIGIN_DSM) return 1;
    if (m->requested_name[0] && strstr(m->requested_name, "dsm:")) return 1;
    return 0;
}

static int name_is_focus(const GwyLoadedModule *m) {
    const char *n;
    if (!m || name_is_dsm(m)) return 0;
    n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    if (!n || !n[0]) return 0;
    if (strstr(n, "mrc_loader")) return 0;
    if (strstr(n, "robotol")) return 1;
    if (m->origin == MODULE_ORIGIN_MRP_MEMBER && strstr(n, ".ext")) return 1;
    return 0;
}

static void emit_fixr9_once(void) {
    if (g_bs.fixr9_logged) return;
    g_bs.fixr9_logged = 1;
    /* DOCUMENTED: FixR9 / bridge — helper uses callee ER_RW; bootstrap not covered. */
    printf("[FIXR9_STAGE_CONTRACT] stage=bootstrap r9_contract=UNKNOWN "
           "evidence=UNKNOWN note=not_extrapolate_helper "
           "bootstrap_entry_r9_gate=blocked\n");
    printf("[FIXR9_STAGE_CONTRACT] stage=mr_c_function_load "
           "r9_contract=CREATES_CALLEE_ER_RW evidence=DOCUMENTED "
           "bootstrap_entry_r9_gate=blocked\n");
    printf("[FIXR9_STAGE_CONTRACT] stage=helper r9_contract=CALLEE_ER_RW "
           "evidence=DOCUMENTED bootstrap_entry_r9_gate=blocked\n");
    printf("[FIXR9_STAGE_CONTRACT] stage=callback r9_contract=UNKNOWN "
           "evidence=UNKNOWN bootstrap_entry_r9_gate=blocked\n");
    fflush(stdout);
}

static void ring_push(uint64_t mid, uint32_t pc, const uint32_t regs[16], uint32_t cpsr) {
    BsRingSlot *s;
    if (g_bs.ring_count < BS_RING) {
        s = &g_bs.ring[g_bs.ring_count++];
    } else {
        s = &g_bs.ring[g_bs.ring_head];
        g_bs.ring_head = (g_bs.ring_head + 1) % BS_RING;
    }
    memset(s, 0, sizeof(*s));
    s->pc = pc;
    s->cpsr = cpsr;
    s->module_id = mid;
    if (regs) memcpy(s->regs, regs, sizeof(s->regs));
}

static int decode_arm_ldr_imm(uint32_t insn, int *rn, int *imm, int *is_load) {
    /* ARM LDR/STR immediate: bits[27:26]=01, bit25(I)=0 */
    if (((insn >> 26) & 3u) != 1u) return 0;
    if (((insn >> 25) & 1u) != 0u) return 0;
    *rn = (int)((insn >> 16) & 0xFu);
    *imm = (int)(insn & 0xFFFu);
    if (((insn >> 23) & 1u) == 0u) *imm = -*imm; /* U=0 → subtract */
    *is_load = ((insn >> 20) & 1u) != 0;
    return 1;
}

static int decode_thumb_ldr_imm(uint32_t insn16, int *rn, int *imm, int *is_load) {
    /* Thumb LDR Rd,[Rn,#imm5*4]: 0110 1 xxx xx nnn ddd */
    if ((insn16 & 0xF800u) == 0x6800u) {
        *rn = (int)((insn16 >> 3) & 7u);
        *imm = (int)(((insn16 >> 6) & 0x1Fu) * 4u);
        *is_load = 1;
        return 1;
    }
    if ((insn16 & 0xF800u) == 0x6000u) {
        *rn = (int)((insn16 >> 3) & 7u);
        *imm = (int)(((insn16 >> 6) & 0x1Fu) * 4u);
        *is_load = 0;
        return 1;
    }
    return 0;
}

static void note_r9_use(uint32_t pc, uint32_t code_base, const char *kind, uint32_t ea) {
    BsR9Use *u;
    if (g_bs.r9_use_count >= BS_R9_USE_MAX) return;
    u = &g_bs.r9_uses[g_bs.r9_use_count++];
    memset(u, 0, sizeof(*u));
    u->pc = pc;
    u->off = (code_base && pc >= code_base) ? (pc - code_base) : 0;
    snprintf(u->access_kind, sizeof(u->access_kind), "%s", kind);
    u->effective_address = ea;
    snprintf(u->address_class, sizeof(u->address_class), "%s",
             (ea >= 0x10000u) ? "HEAP_OR_IMAGE" : "LOW");
    snprintf(u->semantic, sizeof(u->semantic), "%s", "UNKNOWN");
    g_bs.r9_used = 1;
    if (g_bs.r9_use_count == 1) {
        printf("[BOOTSTRAP_R9_USE] first_use_offset=0x%X access_kind=%s "
               "effective_address=0x%X address_class=%s semantic=%s evidence=OBSERVED "
               "bootstrap_entry_r9_gate=blocked\n",
               u->off, kind, ea, u->address_class, u->semantic);
        fflush(stdout);
    }
}

static void maybe_emit_proto(void) {
    const char *r0r = "UNKNOWN";
    if (g_bs.proto_logged || !g_bs.active) return;
    g_bs.proto_logged = 1;
    if (g_bs.entry_r0 == 0) r0r = "ZERO_COMMAND_OR_NULL";
    printf("[BOOTSTRAP_PROTO] r0_role=%s r0=0x%X r1_role=UNKNOWN r1=0x%X "
           "r2_role=UNKNOWN r2=0x%X r3_role=UNKNOWN r3=0x%X stack_args=unknown "
           "evidence=OBSERVED note=no_preset_context "
           "bootstrap_entry_r9_gate=blocked\n",
           r0r, g_bs.entry_r0, g_bs.entry_r1, g_bs.entry_r2, g_bs.entry_r3);
    fflush(stdout);
}

static void maybe_emit_static_base(void) {
    if (g_bs.static_logged || !g_bs.active) return;
    g_bs.static_logged = 1;
    printf("[BOOTSTRAP_STATIC_BASE] base=0x%X source=CALLER_R9 available_before_entry=yes "
           "documented=no used_by_entry=unknown evidence=OBSERVED "
           "note=no_er_rw_delta_guess bootstrap_entry_r9_gate=blocked\n",
           g_bs.caller_r9 ? g_bs.caller_r9 : g_bs.entry_r9);
    if (g_bs.code_base) {
        printf("[BOOTSTRAP_STATIC_BASE] base=0x%X source=CODE_IMAGE available_before_entry=yes "
               "documented=no used_by_entry=unknown evidence=OBSERVED "
               "bootstrap_entry_r9_gate=blocked\n",
               g_bs.code_base);
    }
    fflush(stdout);
}

static void maybe_emit_identity(void) {
    const char *rel = "UNKNOWN";
    uint32_t raw = g_bs.entry_pc;
    uint32_t norm = raw & ~1u;
    uint32_t hdr = g_bs.header_entry;
    uint32_t first = g_bs.first_observed_pc ? g_bs.first_observed_pc : raw;
    if (g_bs.identity_logged || !g_bs.active) return;
    g_bs.identity_logged = 1;
    if (hdr && norm == (hdr & ~1u))
        rel = "DIRECT";
    else if (hdr && norm == ((hdr & ~1u) + 0x14u))
        rel = "THUNK";
    else if (g_bs.code_base && norm > g_bs.code_base + 8u &&
             norm < g_bs.code_base + g_bs.code_size)
        rel = "THUNK";
    printf("[BOOTSTRAP_ENTRY_IDENTITY] raw_return=0x%X normalized=0x%X "
           "function_start=0x%X first_pc=0x%X relation=%s evidence=OBSERVED "
           "bootstrap_entry_r9_gate=blocked\n",
           raw, norm, hdr, first, rel);
    fflush(stdout);
}

static void maybe_emit_path(int faulted) {
    const char *cond = "UNKNOWN";
    if (g_bs.path_logged) return;
    g_bs.path_logged = 1;
    if (g_bs.r9_causality == BS_R9_DIRECTLY_CAUSAL ||
        g_bs.r9_causality == BS_R9_INDIRECTLY_CAUSAL || g_bs.r9_used)
        cond = "R9_REL";
    else if (g_bs.entry_r0 == 0)
        cond = "ARGUMENT";
    else if (faulted && !g_bs.load_reached)
        cond = "UNKNOWN";
    printf("[BOOTSTRAP_PATH] entry_offset=0x%X mr_c_function_load_reached=%s "
           "c_function_new_seen=%s blocking_branch_offset=0x0 condition_source=%s "
           "condition_value=0x%X fault_before_load=%s evidence=OBSERVED "
           "bootstrap_entry_r9_gate=blocked\n",
           g_bs.entry_off, g_bs.load_reached ? "yes" : "no", g_bs.cfn_seen ? "yes" : "no",
           cond, g_bs.entry_r0, (faulted && !g_bs.load_reached) ? "yes" : "no");
    fflush(stdout);
}

static void classify_and_emit(void) {
    BootstrapAbiClass c = BS_ABI_UNKNOWN;
    const char *next = "UNKNOWN";
    const char *gev = "OBSERVED";
    if (g_bs.class_logged || !g_bs.active) return;
    g_bs.class_logged = 1;

    if (g_bs.r9_causality == BS_R9_NOT_USED_BEFORE_FAULT) {
        c = BS_ABI_R9_UNUSED;
        next = "UNKNOWN";
        gev = "OBSERVED";
    } else if (g_bs.r9_causality == BS_R9_DIRECTLY_CAUSAL ||
               g_bs.r9_causality == BS_R9_INDIRECTLY_CAUSAL) {
        /* Observed causal use of R9≠callee ER_RW — do not open temp switch this phase. */
        c = BS_ABI_UNKNOWN;
        next = "UNKNOWN";
        gev = "OBSERVED";
    } else if (g_bs.identity_logged) {
        /* Default lean: DOCUMENTED that helper needs ER_RW later; bootstrap preserves caller. */
        c = BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED;
        next = "PRESERVE_CALLER_R9_POLICY";
        gev = "DOCUMENTED";
    }

    /* If entry looks mid-image and fault after CFN handoff with r9=0 — still preserve policy
     * unless pointer misinterpreted strongly. */
    if (g_bs.entry_off > 0x10u && g_bs.header_entry &&
        (g_bs.entry_pc & ~1u) != (g_bs.header_entry & ~1u) &&
        g_bs.r9_causality == BS_R9_NOT_USED_BEFORE_FAULT) {
        c = BS_ABI_ENTRY_POINTER_MISINTERPRETED;
        next = "ENTRY_POINTER_CORRECTION";
        gev = "OBSERVED";
        /* Keep next UNKNOWN until CROSS_TARGET — plan gate. */
        next = "UNKNOWN";
        gev = "TARGET_OBSERVED";
    }

    /* Prefer preserve-caller when switch blocked and entry used caller R9 then fault with
     * r9 cleared at DSM — R9_USED_BUT_NOT_CAUSAL or NOT_USED → preserve policy DOCUMENTED. */
    if (g_bs.r9_causality == BS_R9_USED_BUT_NOT_CAUSAL ||
        g_bs.r9_causality == BS_R9_NOT_USED_BEFORE_FAULT) {
        c = BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED;
        next = "PRESERVE_CALLER_R9_POLICY";
        gev = "DOCUMENTED";
    }

    g_bs.last_class = c;
    snprintf(g_bs.next_fix, sizeof(g_bs.next_fix), "%s", next);

    printf("[BOOTSTRAP_ABI_CLASS] class=%s r9_causality=%s "
           "bootstrap_entry_r9_gate=blocked module_r9_switch_gate=open "
           "phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked "
           "gate_evidence=%s next_allowed_fix=%s evidence=%s "
           "note=observe_only_no_r9_mutation\n",
           ext_bootstrap_abi_class_name(c), causality_name(g_bs.r9_causality), gev, next, gev);
    printf("[XT_BOOTSTRAP_ORDER] target=jjfb pattern=%s evidence=OBSERVED\n",
           (c == BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED ||
            c == BS_ABI_PRESERVE_CALLER_R9_CROSS_TARGET)
               ? "A"
           : (c == BS_ABI_TEMP_R9_REQUIRED)                 ? "B"
           : (c == BS_ABI_R9_UNUSED)                        ? "C"
           : (c == BS_ABI_ENTRY_POINTER_MISINTERPRETED)     ? "D"
                                                            : "PARTIAL");
    fflush(stdout);
}

void ext_bootstrap_abi_on_code_image(uint32_t guest_addr, uint32_t size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_bootstrap_abi_enabled()) return;
    emit_fixr9_once();
    (void)size;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, guest_addr) : NULL;
    if (m && name_is_focus(m)) {
        g_bs.focus_mid = m->module_id;
        mod_name(m, g_bs.focus_module, sizeof(g_bs.focus_module));
        g_bs.code_base = m->map.guest_code_base ? m->map.guest_code_base : guest_addr;
        g_bs.code_size = m->map.guest_code_size ? m->map.guest_code_size : size;
        g_bs.header_entry = m->entries.header_entry_candidate;
    }
}

void ext_bootstrap_abi_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                      uint32_t rw_base, uint32_t rw_size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_bootstrap_abi_enabled() || !g_bs.active) return;
    (void)p_guest;
    (void)p_len;
    (void)rw_size;
    reg = gwy_ext_loader_bound_registry();
    m = NULL;
    if (reg && helper) {
        m = module_registry_find_by_helper(reg, helper);
        if (!m) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    }
    if (m && (m->module_id == g_bs.focus_mid || name_is_focus(m))) {
        g_bs.cfn_seen = 1;
        if (rw_base) g_bs.load_reached = 1;
        maybe_emit_path(0);
    }
}

void ext_bootstrap_abi_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                    GwyModuleCallKind call_kind, int enter_rc, uint32_t pc,
                                    const uint32_t regs[16]) {
    ModuleRegistry *reg;
    const GwyLoadedModule *callee;
    const GwyLoadedModule *caller;
    uint32_t r9 = 0;
    if (!ext_bootstrap_abi_enabled()) return;
    if (call_kind != GWY_CALL_BOOTSTRAP_ENTRY && call_kind != GWY_CALL_RUNTIME_ENTRY) return;
    reg = gwy_ext_loader_bound_registry();
    callee = reg ? module_registry_find_by_id(reg, callee_module_id) : NULL;
    caller = reg ? module_registry_find_by_id(reg, caller_module_id) : NULL;
    if (!callee || !name_is_focus(callee)) return;
    if (uc) guest_memory_uc_read_r9((struct uc_struct *)uc, &r9);
    else if (regs) r9 = regs[9];

    g_bs.active = 1;
    g_bs.focus_mid = callee->module_id;
    mod_name(callee, g_bs.focus_module, sizeof(g_bs.focus_module));
    g_bs.code_base = callee->map.guest_code_base;
    g_bs.code_size = callee->map.guest_code_size;
    g_bs.header_entry = callee->entries.header_entry_candidate;
    g_bs.entry_pc = pc;
    g_bs.entry_off =
        (g_bs.code_base && pc >= g_bs.code_base) ? ((pc & ~1u) - g_bs.code_base) : 0;
    g_bs.first_observed_pc = pc;
    if (regs) {
        g_bs.entry_r0 = regs[0];
        g_bs.entry_r1 = regs[1];
        g_bs.entry_r2 = regs[2];
        g_bs.entry_r3 = regs[3];
        g_bs.entry_r9 = regs[9];
        g_bs.entry_sp = regs[13];
        g_bs.entry_lr = regs[14];
    }
    g_bs.caller_r9 = r9;
    if (caller && caller->data.start_of_er_rw) g_bs.caller_r9 = caller->data.start_of_er_rw;

    printf("[BOOTSTRAP_R9_ABI] call_kind=%s module=%s module_id=%llu "
           "caller_r9=0x%X callee_er_rw=0x%X effective_r9=0x%X enter_rc=%d "
           "expected_policy=CALLER_DSM_RW_OR_UNSET r9_policy=OBSERVE_ONLY "
           "bootstrap_entry_r9_gate=blocked evidence=OBSERVED\n",
           gwy_module_call_kind_name(call_kind), g_bs.focus_module,
           (unsigned long long)callee_module_id, g_bs.caller_r9, callee->data.start_of_er_rw, r9,
           enter_rc);
    fflush(stdout);

    maybe_emit_proto();
    maybe_emit_static_base();
    maybe_emit_identity();
    emit_fixr9_once();
}

void ext_bootstrap_abi_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                               uint32_t cpsr) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t insn = 0;
    int rn = -1, imm = 0, is_load = 0;
    int thumb;
    if (!ext_bootstrap_abi_enabled() || !g_bs.active) return;
    (void)uc;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    /* Sample focus module and DSM during bootstrap window. */
    if (!m) return;
    if (m->module_id != g_bs.focus_mid && !name_is_dsm(m) && !name_is_focus(m)) return;

    ring_push(module_id, pc, regs, cpsr);

    thumb = (cpsr & (1u << 5)) != 0;
    if (g_bs.uc) {
        uint32_t w = 0;
        if (thumb) {
            if (guest_memory_uc_peek_u32((struct uc_struct *)g_bs.uc, pc & ~1u, &w))
                insn = w & 0xFFFFu;
        } else {
            if (guest_memory_uc_peek_u32((struct uc_struct *)g_bs.uc, pc & ~3u, &w)) insn = w;
        }
    }
    if (thumb) {
        if (decode_thumb_ldr_imm(insn, &rn, &imm, &is_load) && rn == 9 && regs) {
            note_r9_use(pc, g_bs.code_base, is_load ? "READ" : "WRITE", regs[9] + (uint32_t)imm);
        }
    } else {
        if (decode_arm_ldr_imm(insn, &rn, &imm, &is_load) && rn == 9 && regs) {
            note_r9_use(pc, g_bs.code_base, is_load ? "READ" : "WRITE",
                        regs[9] + (uint32_t)imm);
        }
    }
}

void ext_bootstrap_abi_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                    uint32_t access_size, const uint32_t regs[16], uint32_t cpsr) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    uint32_t off = 0;
    int base_reg = -1;
    uint32_t base_val = 0;
    int imm = 0;
    int r9_in = 0;
    char addr_expr[48];
    char insn_desc[64];
    int i, n, idx;
    uint32_t prior_insn = 0;
    int is_load = 0;
    BootstrapR9Causality caus = BS_R9_UNKNOWN;

    if (!ext_bootstrap_abi_enabled()) return;
    (void)uc;
    (void)cpsr;
    if (g_bs.fault_logged) return;
    g_bs.fault_logged = 1;
    /* Fault may arrive after helper leave; keep classification window open. */
    if (g_bs.focus_mid) g_bs.active = 1;

    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, fault_pc & ~1u) : NULL;
    mod_name(m, nm, sizeof(nm));
    if (m && m->map.guest_code_base && fault_pc >= m->map.guest_code_base)
        off = (fault_pc & ~1u) - m->map.guest_code_base;

    snprintf(insn_desc, sizeof(insn_desc), "%s", "UNKNOWN");
    snprintf(addr_expr, sizeof(addr_expr), "%s", "UNKNOWN");

    /* Scan ring backwards for LDR matching fault_addr. */
    n = g_bs.ring_count < BS_RING ? g_bs.ring_count : BS_RING;
    for (i = n - 1; i >= 0 && i >= n - 32; i--) {
        idx = (g_bs.ring_count < BS_RING) ? i : ((g_bs.ring_head + i) % BS_RING);
        {
            const BsRingSlot *s = &g_bs.ring[idx];
            int rn = -1, limm = 0, ld = 0;
            uint32_t ea;
            /* Prefer slots near fault. */
            if (s->pc == fault_pc) continue;
            if (g_bs.uc && ((s->cpsr >> 5) & 1u) == 0) {
                uint32_t w = 0;
                if (guest_memory_uc_peek_u32((struct uc_struct *)g_bs.uc, s->pc & ~3u, &w)) {
                    if (decode_arm_ldr_imm(w, &rn, &limm, &ld)) {
                        ea = s->regs[rn] + (uint32_t)limm;
                        if (ea == fault_addr ||
                            (s->regs[rn] == 0 && (uint32_t)limm == fault_addr)) {
                            base_reg = rn;
                            base_val = s->regs[rn];
                            imm = limm;
                            prior_insn = w;
                            is_load = ld;
                            break;
                        }
                    }
                }
            }
            /* Heuristic: r9==0 and fault_addr looks like imm offset. */
            if (regs && regs[9] == 0 && fault_addr > 0 && fault_addr < 0x1000u) {
                base_reg = 9;
                base_val = 0;
                imm = (int)fault_addr;
                break;
            }
            (void)ea;
        }
    }

    /* Fault-time heuristic: r9==0 and invalid_address in small imm range → SB-relative. */
    if (base_reg < 0 && regs && regs[9] == 0 && fault_addr > 0 && fault_addr < 0x1000u) {
        base_reg = 9;
        base_val = 0;
        imm = (int)fault_addr;
    }
    /* Or any reg+imm = fault */
    if (base_reg < 0 && regs) {
        int rn;
        for (rn = 0; rn < 16; rn++) {
            if (regs[rn] && fault_addr >= regs[rn] && fault_addr - regs[rn] < 0x800u) {
                base_reg = rn;
                base_val = regs[rn];
                imm = (int)(fault_addr - regs[rn]);
                break;
            }
            if (regs[rn] == 0 && fault_addr > 0 && fault_addr < 0x1000u && rn == 0) {
                /* do not assume r0 — leave -1 unless stronger evidence */
            }
        }
    }

    if (base_reg >= 0) {
        if (imm >= 0)
            snprintf(addr_expr, sizeof(addr_expr), "r%d+0x%X", base_reg, (unsigned)imm);
        else
            snprintf(addr_expr, sizeof(addr_expr), "r%d-0x%X", base_reg, (unsigned)(-imm));
        if (base_reg == 9) r9_in = 1;
        if (prior_insn)
            snprintf(insn_desc, sizeof(insn_desc), "%s r?, [r%d, #0x%X] raw=0x%08X",
                     is_load ? "LDR" : "STR", base_reg, imm >= 0 ? (unsigned)imm : (unsigned)(-imm),
                     prior_insn);
        else
            snprintf(insn_desc, sizeof(insn_desc), "RECONSTRUCTED_%s",
                     (base_reg == 9) ? "SB_REL" : "REG_REL");
    }

    if (r9_in && base_val == 0)
        caus = BS_R9_DIRECTLY_CAUSAL;
    else if (r9_in)
        caus = BS_R9_INDIRECTLY_CAUSAL;
    else if (g_bs.r9_used)
        caus = BS_R9_USED_BUT_NOT_CAUSAL;
    else
        caus = BS_R9_NOT_USED_BEFORE_FAULT;
    g_bs.r9_causality = caus;

    /* Fault-time SB-relative use counts as first observed R9 use when ring decode missed it. */
    if (r9_in)
        note_r9_use(fault_pc, g_bs.code_base, is_load ? "LDR" : "MEM", fault_addr);

    /* Ring evidence: CFN-style handoff (r1=0x14) before DSM fault — not robotol load. */
    for (i = 0; i < n; i++) {
        idx = (g_bs.ring_count < BS_RING) ? i : ((g_bs.ring_head + i) % BS_RING);
        if (g_bs.ring[idx].regs[1] == 0x14u && g_bs.ring[idx].regs[0] != 0u) {
            g_bs.cfn_seen = 1;
            break;
        }
    }

    {
        char brname[8];
        if (base_reg >= 0)
            snprintf(brname, sizeof(brname), "r%d", base_reg);
        else
            snprintf(brname, sizeof(brname), "%s", "unknown");
        printf("[BOOTSTRAP_FAULT] pc=0x%X pc_module=%s pc_offset=0x%X instruction=%s "
               "access=READ size=%u address_expr=%s base_reg=%s base_value=0x%X "
               "invalid_address=0x%X r9=0x%X r9_in_dataflow=%s causality=%s evidence=OBSERVED "
               "bootstrap_entry_r9_gate=blocked note=no_hardcoded_imm\n",
               fault_pc, nm[0] ? nm : "?", off, insn_desc, access_size, addr_expr, brname,
               base_val, fault_addr, regs ? regs[9] : 0, r9_in ? "yes" : "no",
               causality_name(caus));
        fflush(stdout);
    }

    /* Emit classification before ring dump so kill/truncation cannot drop gates. */
    maybe_emit_path(1);
    classify_and_emit();

    /* Dump last 24 ring slots only (observe-only history). */
    {
        int dump = n < 24 ? n : 24;
        int start = n - dump;
        for (i = start; i < n; i++) {
            idx = (g_bs.ring_count < BS_RING) ? i : ((g_bs.ring_head + i) % BS_RING);
            {
                const BsRingSlot *s = &g_bs.ring[idx];
                printf("[BOOTSTRAP_FAULT_RING] i=%d pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "r9=0x%X sp=0x%X lr=0x%X module_id=%llu\n",
                       i - start, s->pc, s->regs[0], s->regs[1], s->regs[2], s->regs[3],
                       s->regs[9], s->regs[13], s->regs[14],
                       (unsigned long long)s->module_id);
            }
        }
        fflush(stdout);
    }
}
