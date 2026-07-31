#include "gwy_launcher/product_p11_case9_trace.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P11_RING 64
#define P11_SLICE_CAP 4096
#define P11_STACK_WORDS 0x60u / 4u

typedef struct P11Insn {
    uint64_t request_id;
    uint32_t seq;
    uint32_t pc;
    uint32_t size;
    uint8_t bytes[8];
    uint32_t r[13];
    uint32_t sp;
    uint32_t lr;
    uint32_t cpsr;
    uint32_t r9;
    uint32_t entry_sp;
    int32_t sp_delta;
    char kind[24]; /* branch/load/store/alu/other */
    char note[48];
} P11Insn;

typedef struct P11State {
    int known;
    int enabled;
    char reports_dir[260];
    uint64_t active_request_id;
    uint32_t case9_handler;
    uint32_t entry_sp;
    uint32_t entry_lr;
    uint32_t entry_r[13];
    int case9_active;
    int case9_returned;
    int dumps_done;
    int reg_done;
    uint32_t slice_n;
    P11Insn slice[P11_SLICE_CAP];
    P11Insn ring[P11_RING];
    unsigned ring_n;
    unsigned ring_next;
    uint32_t global_seq;
    /* Last writer tracking for provenance of 0x1E205-bearing regs */
    uint32_t last_write_pc[16];
    uint32_t last_write_val[16];
    char last_write_note[16][40];
#ifdef GWY_HAVE_UNICORN
    uc_hook code_hook;
    int code_hook_on;
    void *hook_uc;
#endif
    FILE *reg_csv;
    FILE *slice_csv;
    FILE *fault_csv;
} P11State;

static P11State g_p11;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int product_p11_enabled(void) {
    if (g_p11.known) return g_p11.enabled;
    g_p11.enabled = env1("JJFB_P11_MODE");
    g_p11.known = 1;
    if (g_p11.enabled) {
        const char *d = getenv("JJFB_P11_REPORTS_DIR");
        snprintf(g_p11.reports_dir, sizeof(g_p11.reports_dir), "%s",
                 d && d[0] ? d : "reports");
    }
    return g_p11.enabled;
}

static void path_join(char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", g_p11.reports_dir[0] ? g_p11.reports_dir : "reports", name);
}

static FILE *open_report(const char *name, const char *header) {
    char path[320];
    FILE *f;
    path_join(path, sizeof(path), name);
    f = fopen(path, "wb");
    if (f && header) {
        fputs(header, f);
        fflush(f);
    }
    return f;
}

static void read_regs(void *uc, uint32_t r[13], uint32_t *sp, uint32_t *lr, uint32_t *pc,
                      uint32_t *cpsr, uint32_t *r9) {
#ifdef GWY_HAVE_UNICORN
    uc_engine *u = (uc_engine *)uc;
    int i;
    if (!u) return;
    for (i = 0; i < 13; i++) uc_reg_read(u, UC_ARM_REG_R0 + i, &r[i]);
    if (sp) uc_reg_read(u, UC_ARM_REG_SP, sp);
    if (lr) uc_reg_read(u, UC_ARM_REG_LR, lr);
    if (pc) uc_reg_read(u, UC_ARM_REG_PC, pc);
    if (cpsr) uc_reg_read(u, UC_ARM_REG_CPSR, cpsr);
    if (r9) uc_reg_read(u, UC_ARM_REG_R9, r9);
#else
    (void)uc;
    (void)r;
    (void)sp;
    (void)lr;
    (void)pc;
    (void)cpsr;
    (void)r9;
#endif
}

static void dump_bin(void *uc, uint32_t begin, uint32_t end, const char *name) {
#ifdef GWY_HAVE_UNICORN
    char path[320];
    FILE *f;
    uint32_t len;
    uint8_t *buf;
    if (!uc || end <= begin) return;
    len = end - begin;
    buf = (uint8_t *)malloc(len);
    if (!buf) return;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, begin, buf, len)) {
        free(buf);
        return;
    }
    path_join(path, sizeof(path), name);
    f = fopen(path, "wb");
    if (f) {
        fwrite(buf, 1, len, f);
        fclose(f);
        printf("[JJFB_P11] dump=%s begin=0x%X end=0x%X len=%u evidence=OBSERVED\n", name, begin,
               end, len);
        fflush(stdout);
    }
    free(buf);
#else
    (void)uc;
    (void)begin;
    (void)end;
    (void)name;
#endif
}

static void classify_thumb(const uint8_t *b, uint32_t size, uint32_t pc, const uint32_t r[13],
                           uint32_t sp, uint32_t entry_sp, char *kind, size_t kn, char *note,
                           size_t nn, int32_t *sp_delta_out) {
    uint16_t h0;
    (void)pc;
    (void)r;
    kind[0] = 0;
    note[0] = 0;
    if (sp_delta_out) *sp_delta_out = (int32_t)(sp - entry_sp);
    if (!b || size < 2) {
        snprintf(kind, kn, "other");
        return;
    }
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    /* PUSH */
    if ((h0 & 0xFE00u) == 0xB400u) {
        snprintf(kind, kn, "store");
        snprintf(note, nn, "PUSH");
        return;
    }
    /* POP */
    if ((h0 & 0xFE00u) == 0xBC00u) {
        snprintf(kind, kn, "load");
        snprintf(note, nn, "POP");
        return;
    }
    /* ADD/SUB SP imm */
    if ((h0 & 0xFF80u) == 0xB080u) {
        snprintf(kind, kn, "alu");
        snprintf(note, nn, "SUB_SP");
        return;
    }
    if ((h0 & 0xFF80u) == 0xB000u) {
        snprintf(kind, kn, "alu");
        snprintf(note, nn, "ADD_SP");
        return;
    }
    /* LDR/STR [SP,#imm] */
    if ((h0 & 0xF800u) == 0x9800u) {
        uint32_t imm = (h0 & 0xFFu) << 2;
        snprintf(kind, kn, "load");
        snprintf(note, nn, "LDR_SP+0x%X", imm);
        return;
    }
    if ((h0 & 0xF800u) == 0x9000u) {
        uint32_t imm = (h0 & 0xFFu) << 2;
        snprintf(kind, kn, "store");
        snprintf(note, nn, "STR_SP+0x%X", imm);
        return;
    }
    /* BX Rm */
    if ((h0 & 0xFF87u) == 0x4700u) {
        snprintf(kind, kn, "branch");
        snprintf(note, nn, "BX_R%u", (h0 >> 3) & 7u);
        return;
    }
    /* BLX Rm */
    if ((h0 & 0xFF87u) == 0x4780u) {
        snprintf(kind, kn, "branch");
        snprintf(note, nn, "BLX_R%u", (h0 >> 3) & 7u);
        return;
    }
    /* B cond / B */
    if ((h0 & 0xF000u) == 0xD000u || (h0 & 0xF800u) == 0xE000u) {
        snprintf(kind, kn, "branch");
        snprintf(note, nn, "B");
        return;
    }
    if (size >= 4) {
        uint16_t h1 = (uint16_t)(b[2] | (b[3] << 8));
        if ((h0 & 0xF800u) == 0xF000u && (h1 & 0xC000u) == 0xC000u) {
            snprintf(kind, kn, "branch");
            snprintf(note, nn, ((h1 & 0x1000u) ? "BL" : "BLX"));
            return;
        }
    }
    /* LDR/STR imm [Rn] */
    if ((h0 & 0xF800u) == 0x6800u || (h0 & 0xF800u) == 0x6000u) {
        snprintf(kind, kn, ((h0 & 0xF800u) == 0x6800u) ? "load" : "store");
        snprintf(note, nn, "LDRSTR_Rn");
        return;
    }
    snprintf(kind, kn, "other");
}

static void ring_push(const P11Insn *e) {
    g_p11.ring[g_p11.ring_next] = *e;
    g_p11.ring_next = (g_p11.ring_next + 1u) % P11_RING;
    if (g_p11.ring_n < P11_RING) g_p11.ring_n++;
}

static void note_reg_write(uint32_t rd, uint32_t pc, uint32_t val, const char *note) {
    if (rd >= 16u) return;
    g_p11.last_write_pc[rd] = pc;
    g_p11.last_write_val[rd] = val;
    snprintf(g_p11.last_write_note[rd], sizeof(g_p11.last_write_note[rd]), "%s",
             note ? note : "?");
}

#ifdef GWY_HAVE_UNICORN
static void p11_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    P11Insn e;
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0, r9 = 0;
    uint32_t sz = size;
    (void)user;
    if (!product_p11_enabled()) return;
    memset(&e, 0, sizeof(e));
    read_regs(uc, r, &sp, &lr, NULL, &cpsr, &r9);
    if (sz == 0 || sz > 8) sz = 8;
    e.request_id = g_p11.active_request_id;
    e.seq = ++g_p11.global_seq;
    e.pc = (uint32_t)address;
    e.size = sz;
    (void)uc_mem_read(uc, address, e.bytes, sz);
    memcpy(e.r, r, sizeof(r));
    e.sp = sp;
    e.lr = lr;
    e.cpsr = cpsr;
    e.r9 = r9;
    e.entry_sp = g_p11.entry_sp;
    classify_thumb(e.bytes, sz, e.pc, r, sp, g_p11.entry_sp, e.kind, sizeof(e.kind), e.note,
                   sizeof(e.note), &e.sp_delta);
    /* Track MOV Rd,imm / LDR that may introduce 0x1E205 */
    {
        uint16_t h0 = (uint16_t)(e.bytes[0] | (e.bytes[1] << 8));
        if ((h0 & 0xF800u) == 0x2000u) { /* MOVS Rd,#imm8 */
            uint32_t rd = (h0 >> 8) & 7u;
            uint32_t imm = h0 & 0xFFu;
            note_reg_write(rd, e.pc, imm, "MOVS_imm8");
        }
        if ((h0 & 0xF800u) == 0x9800u) { /* LDR Rd,[SP,#imm] */
            uint32_t rd = (h0 >> 8) & 7u;
            note_reg_write(rd, e.pc, r[rd], "LDR_SP");
        }
        if ((h0 & 0xFF00u) == 0x4600u) { /* MOV Rd,Rm high */
            uint32_t rd = ((h0 >> 7) & 1u) << 3 | (h0 & 7u);
            uint32_t rm = (h0 >> 3) & 0xFu;
            if (rd < 16u && rm < 13u) note_reg_write(rd, e.pc, r[rm], "MOV_Rm");
        }
    }
    for (int i = 0; i < 13; i++) {
        if (r[i] == 0x1E205u || r[i] == 0x1E209u)
            note_reg_write((uint32_t)i, e.pc, r[i], "REG_HOLDS_EVENTISH");
    }
    ring_push(&e);
    if (g_p11.case9_active && g_p11.slice_n < P11_SLICE_CAP) {
        g_p11.slice[g_p11.slice_n++] = e;
        if (g_p11.slice_csv) {
            fprintf(g_p11.slice_csv,
                    "%llu,%u,0x%X,%u,%02X%02X%02X%02X,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,0x%X\n",
                    (unsigned long long)e.request_id, e.seq, e.pc, e.size, e.bytes[0], e.bytes[1],
                    e.bytes[2], e.bytes[3], e.kind, e.note, e.r[0], e.r[1], e.r[2], e.r[3], e.r[4],
                    e.r[5], e.r[6], e.r[7], e.r[8], e.r[9], e.r[10], e.r[11], e.r[12], e.sp, e.lr,
                    e.cpsr, e.sp_delta, e.entry_sp);
            fflush(g_p11.slice_csv);
        }
    }
}
#endif

static void ensure_code_hook(void *uc, uint32_t begin, uint32_t end) {
#ifdef GWY_HAVE_UNICORN
    if (!product_p11_enabled() || !uc) return;
    if (g_p11.code_hook_on && g_p11.hook_uc == uc) return;
    if (g_p11.code_hook_on && g_p11.hook_uc) {
        uc_hook_del((uc_engine *)g_p11.hook_uc, g_p11.code_hook);
        g_p11.code_hook_on = 0;
    }
    if (uc_hook_add((uc_engine *)uc, &g_p11.code_hook, UC_HOOK_CODE, (void *)p11_on_code, NULL,
                    begin, end) == UC_ERR_OK) {
        g_p11.code_hook_on = 1;
        g_p11.hook_uc = uc;
        printf("[JJFB_P11] code_hook begin=0x%X end=0x%X evidence=OBSERVED\n", begin, end);
        fflush(stdout);
    }
#else
    (void)uc;
    (void)begin;
    (void)end;
#endif
}

void product_p11_on_10102_register(void *uc, uint32_t family, uint32_t handler, uint32_t caller_pc,
                                   uint32_t lr, int32_t status_ret, const char *owner_module,
                                   uint64_t owner_module_id, uint64_t owner_generation,
                                   uint32_t er_rw, uint32_t code_base, uint32_t code_size) {
    uint32_t r[13], sp = 0, pc = 0, cpsr = 0, r9 = 0;
    uint32_t stack[P11_STACK_WORDS];
    unsigned i;
    static uint32_t reg_id;
    if (!product_p11_enabled()) return;
    if (g_p11.reg_done && handler == g_p11.case9_handler) return;
    memset(r, 0, sizeof(r));
    memset(stack, 0, sizeof(stack));
    read_regs(uc, r, &sp, NULL, &pc, &cpsr, &r9);
    if (!er_rw) er_rw = r9;
#ifdef GWY_HAVE_UNICORN
    if (uc && sp) {
        for (i = 0; i < P11_STACK_WORDS; i++) {
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + i * 4u, &stack[i]);
        }
    }
#endif
    reg_id++;
    if (!g_p11.reg_csv) {
        g_p11.reg_csv = open_report(
            "p11_10102_registration_trace.csv",
            "registration_id,family,handler,caller_pc,lr,pc,cpsr,r9,sp,status_ret,owner_module,"
            "owner_module_id,owner_generation,er_rw,code_base,code_size,"
            "r0,r1,r2,r3,r4,r5,r6,r7,r8,r10,r11,r12,"
            "sp00,sp04,sp08,sp0c,sp10,sp14,sp18,sp1c,sp20,sp24,sp28,sp2c,sp30,sp34,sp38,sp3c,"
            "sp40,sp44,sp48,sp4c,sp50,sp54,sp58,sp5c\n");
    }
    if (g_p11.reg_csv) {
        fprintf(g_p11.reg_csv,
                "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%s,%llu,%llu,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X",
                reg_id, family, handler, caller_pc, lr, pc, cpsr, r9, sp, (int)status_ret,
                owner_module ? owner_module : "?", (unsigned long long)owner_module_id,
                (unsigned long long)owner_generation, er_rw, code_base, code_size, r[0], r[1], r[2],
                r[3], r[4], r[5], r[6], r[7], r[8], r[10], r[11], r[12]);
        for (i = 0; i < P11_STACK_WORDS; i++) fprintf(g_p11.reg_csv, ",0x%X", stack[i]);
        fputc('\n', g_p11.reg_csv);
        fflush(g_p11.reg_csv);
    }
    g_p11.case9_handler = handler;
    g_p11.reg_done = 1;
    printf("[JJFB_P11] REG10102 id=%u family=0x%X handler=0x%X owner=%s er_rw=0x%X r2=0x%X "
           "r3=0x%X sp=0x%X lr=0x%X evidence=OBSERVED\n",
           reg_id, family, handler, owner_module ? owner_module : "?", er_rw, r[2], r[3], sp, lr);
    fflush(stdout);
    /* Arm persistent code hook over owner module once known. */
    if (code_base && code_size)
        ensure_code_hook(uc, code_base, code_base + code_size);
    else if (handler)
        ensure_code_hook(uc, (handler & ~1u) - 0x1000u, (handler & ~1u) + 0x20000u);
}

void product_p11_on_family_request(uint64_t request_id, uint32_t event_code, uint32_t app,
                                   uint32_t handler, uint32_t caller_pc, uint32_t lr) {
    if (!product_p11_enabled()) return;
    if (event_code == 0x1E209u && app == 9u) {
        g_p11.active_request_id = request_id ? request_id : 1ull;
        printf("[JJFB_P11] FAMILY_REQ request_id=%llu event=0x%X app=%u handler=0x%X "
               "caller=0x%X lr=0x%X evidence=OBSERVED\n",
               (unsigned long long)g_p11.active_request_id, event_code, app, handler, caller_pc,
               lr);
        fflush(stdout);
    }
}

void product_p11_case9_deliver_begin(void *uc, uint64_t request_id, uint32_t event_code,
                                     uint32_t app, uint32_t handler, uint32_t r0, uint32_t r1,
                                     uint32_t r2, uint32_t r3, uint32_t r9, uint32_t stop) {
    uint32_t regs[13], sp = 0, lr = 0, pc = 0, cpsr = 0, rr9 = 0;
    if (!product_p11_enabled()) return;
    if (!(event_code == 0x1E209u && app == 9u)) return;
    if (request_id) g_p11.active_request_id = request_id;
    read_regs(uc, regs, &sp, &lr, &pc, &cpsr, &rr9);
    g_p11.entry_sp = sp;
    g_p11.entry_lr = lr;
    memcpy(g_p11.entry_r, regs, sizeof(regs));
    g_p11.case9_active = 1;
    g_p11.case9_returned = 0;
    g_p11.slice_n = 0;
    if (!g_p11.slice_csv) {
        g_p11.slice_csv = open_report(
            "p11_case9_dynamic_slice.csv",
            "request_id,seq,pc,size,raw4,kind,note,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,"
            "sp,lr,cpsr,sp_delta,entry_sp\n");
    }
    if (!g_p11.dumps_done) {
        dump_bin(uc, 0x30D280u, 0x30D480u, "p11_runtime_case9_30d280_30d480.bin");
        dump_bin(uc, 0x2D95C0u, 0x2D9660u, "p11_runtime_fault_2d95c0_2d9660.bin");
        g_p11.dumps_done = 1;
    }
    ensure_code_hook(uc, 0x2D8000u, 0x320000u);
    printf("[JJFB_P11] CASE9_ENTER request_id=%llu handler=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "r9=0x%X entry_sp=0x%X stop=0x%X evidence=OBSERVED\n",
           (unsigned long long)g_p11.active_request_id, handler, r0, r1, r2, r3, r9 ? r9 : rr9,
           g_p11.entry_sp, stop);
    fflush(stdout);
}

void product_p11_case9_deliver_end(void *uc, uint64_t request_id, int ok, unsigned uc_err,
                                   uint32_t pc_after, int32_t ret) {
    if (!product_p11_enabled()) return;
    if (!g_p11.case9_active) return;
    (void)uc;
    (void)request_id;
    g_p11.case9_active = 0;
    g_p11.case9_returned = ok ? 1 : 0;
    printf("[JJFB_P11] CASE9_LEAVE ok=%d uc_err=%u pc_after=0x%X ret=%d slice_n=%u "
           "evidence=OBSERVED\n",
           ok, uc_err, pc_after, (int)ret, g_p11.slice_n);
    fflush(stdout);
}

void product_p11_on_scheduler_tick(const char *phase) {
    if (!product_p11_enabled()) return;
    printf("[JJFB_P11] SCHED phase=%s case9_returned=%d evidence=OBSERVED\n",
           phase ? phase : "?", g_p11.case9_returned);
    fflush(stdout);
}

static void write_fault_context(void *uc, uint32_t fault_pc, uint64_t address, uint32_t size,
                                uint32_t access_type) {
    char path[320];
    FILE *md;
    unsigned i, n, idx;
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0, r9 = 0;
    int bearing = -1;
    read_regs(uc, r, &sp, &lr, NULL, &cpsr, &r9);
    if (!g_p11.fault_csv) {
        g_p11.fault_csv = open_report(
            "p11_late_fault_ring.csv",
            "seq,pc,raw4,kind,note,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr,cpsr,"
            "request_id\n");
    }
    n = g_p11.ring_n;
    idx = (g_p11.ring_next + P11_RING - n) % P11_RING;
    for (i = 0; i < n; i++) {
        P11Insn *e = &g_p11.ring[(idx + i) % P11_RING];
        if (g_p11.fault_csv) {
            fprintf(g_p11.fault_csv,
                    "%u,0x%X,%02X%02X%02X%02X,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%llu\n",
                    e->seq, e->pc, e->bytes[0], e->bytes[1], e->bytes[2], e->bytes[3], e->kind,
                    e->note, e->r[0], e->r[1], e->r[2], e->r[3], e->r[4], e->r[5], e->r[6],
                    e->r[7], e->r[8], e->r[9], e->r[10], e->r[11], e->r[12], e->sp, e->lr,
                    e->cpsr, (unsigned long long)e->request_id);
        }
    }
    if (g_p11.fault_csv) fflush(g_p11.fault_csv);

    for (i = 0; i < 13; i++) {
        if (r[i] == (uint32_t)address || r[i] == 0x1E205u) {
            bearing = (int)i;
            break;
        }
    }

    path_join(path, sizeof(path), "p11_late_fault_context.md");
    md = fopen(path, "wb");
    if (!md) return;
    fprintf(md, "# P11 Late Fault Context\n\n");
    fprintf(md, "- fault_pc: `0x%X`\n", fault_pc);
    fprintf(md, "- fault_address: `0x%llX`\n", (unsigned long long)address);
    fprintf(md, "- fault_size: %u\n", size);
    fprintf(md, "- access_type: %u\n", access_type);
    fprintf(md, "- case9_returned: %d\n", g_p11.case9_returned);
    fprintf(md, "- case9_active_at_fault: %d\n", g_p11.case9_active);
    fprintf(md, "- request_id: %llu\n", (unsigned long long)g_p11.active_request_id);
    fprintf(md, "- ring_depth: %u\n\n", g_p11.ring_n);
    fprintf(md, "## Registers at fault\n\n");
    for (i = 0; i < 13; i++) fprintf(md, "- R%u: `0x%X`\n", i, r[i]);
    fprintf(md, "- SP: `0x%X` LR: `0x%X` CPSR: `0x%X` R9: `0x%X`\n\n", sp, lr, cpsr, r9);

    fprintf(md, "## Role of 0x1E205 / fault address\n\n");
    if ((uint32_t)address == 0x1E205u || (uint32_t)address == 0x1E209u) {
        fprintf(md, "Fault address equals family event-code family (`0x1E209` band).\n");
        fprintf(md, "Documented producer: `0x1E205 = 0x1E209 - 4` used as `sendAppEvent(0x10133)` "
                    "arg1 (EVENT_305E09 / free-object path).\n\n");
    }
    if (bearing >= 0) {
        fprintf(md, "Register **R%d** holds `0x%X` at fault.\n", bearing, r[bearing]);
        fprintf(md, "Last observed write to R%d: pc=`0x%X` val=`0x%X` note=`%s`\n\n", bearing,
                g_p11.last_write_pc[bearing], g_p11.last_write_val[bearing],
                g_p11.last_write_note[bearing]);
        fprintf(md, "### Provenance chain\n\n");
        fprintf(md, "```\n0x%X\n← present in R%d at fault pc 0x%X\n", (uint32_t)address, bearing,
                fault_pc);
        fprintf(md, "← last write @ 0x%X (%s) value=0x%X\n", g_p11.last_write_pc[bearing],
                g_p11.last_write_note[bearing], g_p11.last_write_val[bearing]);
        if ((uint32_t)address == 0x1E205u) {
            fprintf(md, "← event_code family: 0x1E209 - 4 (platform 0x10133 object-id / free key)\n");
            fprintf(md, "← NOT a mapped guest code/data pointer (no mapping near 0x1E000)\n");
        }
        fprintf(md, "```\n\n");
        if (strstr(g_p11.last_write_note[bearing], "BX") ||
            strstr(g_p11.last_write_note[bearing], "BLX") ||
            (fault_pc && access_type /* fetch often 16/UC_MEM_FETCH_UNMAPPED */)) {
            /* classify */
        }
        fprintf(md, "**Classification:** ");
        if ((access_type & 16) || (access_type == 16))
            fprintf(md, "likely **indirect fetch / jump target** (event code treated as address)\n");
        else if (access_type & 2)
            fprintf(md, "likely **write target** through event-code-as-pointer\n");
        else
            fprintf(md, "likely **read target** through event-code-as-pointer / data pointer misuse\n");
    } else {
        fprintf(md, "No GPR currently equals fault address; may be PC-relative or computed EA.\n");
        fprintf(md, "Check ring for LDR that formed EA=`0x%llX`.\n", (unsigned long long)address);
    }
    fprintf(md, "\n## SP window (±0x40)\n\n");
#ifdef GWY_HAVE_UNICORN
    if (uc && sp) {
        uint32_t base = sp > 0x40u ? sp - 0x40u : 0u;
        uint32_t off;
        for (off = 0; off < 0x80u; off += 4u) {
            uint32_t w = 0;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, base + off, &w))
                fprintf(md, "- [0x%X] = `0x%X`%s\n", base + off, w,
                        (base + off == sp) ? "  ← SP" : "");
        }
    }
#endif
    fclose(md);
    printf("[JJFB_P11] FAULT_CONTEXT written pc=0x%X addr=0x%llX bearing_reg=%d "
           "evidence=OBSERVED\n",
           fault_pc, (unsigned long long)address, bearing);
    fflush(stdout);
}

void product_p11_on_mem_fault(void *uc, uint32_t access_type, uint64_t address, uint32_t size,
                              int64_t value) {
    uint32_t pc = 0;
    (void)value;
    if (!product_p11_enabled()) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
#else
    (void)uc;
#endif
    /* Focus late Case-9 / event-code faults; still record any fault after Case-9. */
    if ((uint32_t)address == 0x1E205u || (uint32_t)address == 0x1E209u ||
        (pc >= 0x2D95C0u && pc <= 0x2D9660u) || g_p11.case9_returned || g_p11.case9_active) {
        write_fault_context(uc, pc, address, size, access_type);
    }
}
