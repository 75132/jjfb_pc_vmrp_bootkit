#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <string.h>

#include <unicorn/unicorn.h>

static int range_in_regions(const VmRuntime *rt, uint32_t addr, size_t size, GwyMemoryAccess access) {
    size_t n = 0;
    size_t i;
    const VmMemoryRegion *regs;
    uint64_t end;
    (void)access;

    if (!rt || size == 0) return 0;
    end = (uint64_t)addr + (uint64_t)size;
    if (end < (uint64_t)addr) return 0;

    regs = vm_runtime_regions(rt, &n);
    if (!regs || n == 0) return 0;

    for (i = 0; i < n; i++) {
        uint64_t r0, r1;
        if (!regs[i].used) continue;
        r0 = regs[i].base;
        r1 = (uint64_t)regs[i].base + (uint64_t)regs[i].size;
        if (addr >= r0 && end <= r1) {
            if (access == GWY_MEM_WRITE && (regs[i].permissions & 0x2) == 0) return 0;
            if (access == GWY_MEM_READ && (regs[i].permissions & 0x1) == 0) return 0;
            return 1;
        }
    }
    return 0;
}

LauncherStatus guest_memory_validate_range(VmRuntime *rt,
                                           uint32_t guest_address,
                                           size_t size,
                                           GwyMemoryAccess access,
                                           LauncherError *err) {
    launcher_error_clear(err);
    if (!rt) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_memory", "null runtime", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!vm_runtime_is_running(rt) || !vm_runtime_uc(rt)) {
        launcher_error_set(err, L_ERR_STATE, "guest_memory", "runtime not started", NULL);
        return L_ERR_STATE;
    }
    if (size == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_memory", "zero size", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!range_in_regions(rt, guest_address, size, access)) {
        launcher_error_set(err, L_ERR_GUEST_FAULT, "guest_memory", "out of mapped regions", NULL);
        return L_ERR_GUEST_FAULT;
    }
    return L_OK;
}

LauncherStatus guest_memory_read(VmRuntime *rt,
                                 uint32_t guest_address,
                                 void *buffer,
                                 size_t size,
                                 LauncherError *err) {
    LauncherStatus st;
    uc_err uerr;

    if (!buffer) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_memory", "null buffer", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = guest_memory_validate_range(rt, guest_address, size, GWY_MEM_READ, err);
    if (st != L_OK) return st;

    uerr = uc_mem_read(vm_runtime_uc(rt), guest_address, buffer, size);
    if (uerr != UC_ERR_OK) {
        launcher_error_set(err, L_ERR_GUEST_FAULT, "guest_memory", "uc_mem_read failed",
                           uc_strerror(uerr));
        return L_ERR_GUEST_FAULT;
    }
    return L_OK;
}

LauncherStatus guest_memory_write(VmRuntime *rt,
                                  uint32_t guest_address,
                                  const void *buffer,
                                  size_t size,
                                  LauncherError *err) {
    LauncherStatus st;
    uc_err uerr;

    if (!buffer) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_memory", "null buffer", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = guest_memory_validate_range(rt, guest_address, size, GWY_MEM_WRITE, err);
    if (st != L_OK) return st;

    uerr = uc_mem_write(vm_runtime_uc(rt), guest_address, buffer, size);
    if (uerr != UC_ERR_OK) {
        launcher_error_set(err, L_ERR_GUEST_FAULT, "guest_memory", "uc_mem_write failed",
                           uc_strerror(uerr));
        return L_ERR_GUEST_FAULT;
    }
    return L_OK;
}

LauncherStatus guest_memory_read_cstring(VmRuntime *rt,
                                         uint32_t guest_address,
                                         char *buffer,
                                         size_t capacity,
                                         LauncherError *err) {
    size_t i;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!buffer || capacity == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_memory", "bad cstring buffer", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    buffer[0] = '\0';

    for (i = 0; i + 1 < capacity; i++) {
        char c = 0;
        st = guest_memory_read(rt, guest_address + (uint32_t)i, &c, 1, err);
        if (st != L_OK) return st;
        buffer[i] = c;
        if (c == '\0') return L_OK;
    }
    buffer[capacity - 1] = '\0';
    {
        char extra = 0;
        st = guest_memory_read(rt, guest_address + (uint32_t)(capacity - 1), &extra, 1, err);
        if (st != L_OK) {
            launcher_error_clear(err);
            return L_OK;
        }
        if (extra != '\0') {
            launcher_error_set(err, L_ERR_BOUNDS, "guest_memory", "cstring truncated", NULL);
            return L_ERR_BOUNDS;
        }
    }
    return L_OK;
}

int guest_memory_uc_peek(struct uc_struct *uc, uint32_t guest_address, void *buffer, size_t size) {
    if (!uc || !buffer || size == 0) return 0;
    if (uc_mem_read((uc_engine *)uc, guest_address, buffer, size) != UC_ERR_OK) return 0;
    return 1;
}

int guest_memory_uc_peek_u32(struct uc_struct *uc, uint32_t guest_address, uint32_t *out) {
    uint32_t word = 0;
    if (!out) return 0;
    if (!guest_memory_uc_peek(uc, guest_address, &word, sizeof(word))) return 0;
    *out = word;
    return 1;
}

int guest_memory_uc_poke(struct uc_struct *uc, uint32_t guest_address, const void *buffer,
                         size_t size) {
    if (!uc || !buffer || size == 0) return 0;
    if (uc_mem_write((uc_engine *)uc, guest_address, buffer, size) != UC_ERR_OK) return 0;
    return 1;
}

int guest_memory_uc_poke_u32(struct uc_struct *uc, uint32_t guest_address, uint32_t value) {
    return guest_memory_uc_poke(uc, guest_address, &value, sizeof(value));
}

int guest_memory_uc_run_entry_ex(struct uc_struct *uc, uint32_t start_pc, uint32_t stop_addr,
                                 uint64_t insn_limit, const GwyUcEntryAbi *abi,
                                 GwyUcEntryRunOut *out) {
#ifdef GWY_HAVE_UNICORN
    uc_engine *u = (uc_engine *)uc;
    uint32_t saved[17];
    int i;
    uc_err err;
    const int regs_idx[17] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,  UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8,  UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,
        UC_ARM_REG_PC,  UC_ARM_REG_CPSR};
    if (out) memset(out, 0, sizeof(*out));
    if (!uc || !start_pc) {
        if (out) snprintf(out->end_reason, sizeof(out->end_reason), "bad_args");
        return 0;
    }
    if (insn_limit == 0) insn_limit = 200000ull;
    for (i = 0; i < 17; i++) {
        saved[i] = 0;
        uc_reg_read(u, regs_idx[i], &saved[i]);
    }
    if (abi && abi->mirror_full) {
        for (i = 0; i < 13; i++) uc_reg_write(u, regs_idx[i], &abi->mirror_r[i]);
        uc_reg_write(u, UC_ARM_REG_SP, &abi->mirror_sp);
        uc_reg_write(u, UC_ARM_REG_CPSR, &abi->mirror_cpsr);
    }
    if (abi) {
        if (abi->set_r0) uc_reg_write(u, UC_ARM_REG_R0, &abi->r0);
        if (abi->set_r1) uc_reg_write(u, UC_ARM_REG_R1, &abi->r1);
        if (abi->set_r2) uc_reg_write(u, UC_ARM_REG_R2, &abi->r2);
        if (abi->set_r3) uc_reg_write(u, UC_ARM_REG_R3, &abi->r3);
        if (abi->set_lr) uc_reg_write(u, UC_ARM_REG_LR, &abi->lr);
    } else {
        uint32_t z = 0;
        (void)z;
    }
    /* Thumb bit from start_pc LSB (CROSS_TARGET guest handlers are often Thumb). */
    {
        uint32_t cpsr = saved[16];
        uint32_t pc_run = start_pc & ~1u;
        uint32_t stop_run = stop_addr & ~1u;
        if (start_pc & 1u)
            cpsr |= (1u << 5);
        else
            cpsr &= ~(1u << 5);
        uc_reg_write(u, UC_ARM_REG_CPSR, &cpsr);
        err = uc_emu_start(u, pc_run, stop_run, 0, insn_limit);
    }
    if (out) {
        out->uc_err = (unsigned)err;
        uc_reg_read(u, UC_ARM_REG_PC, &out->pc_after);
        uc_reg_read(u, UC_ARM_REG_R0, &out->r0_after);
        uc_reg_read(u, UC_ARM_REG_R1, &out->r1_after);
        uc_reg_read(u, UC_ARM_REG_R9, &out->r9_after);
        uc_reg_read(u, UC_ARM_REG_SP, &out->sp_after);
        uc_reg_read(u, UC_ARM_REG_LR, &out->lr_after);
        uc_reg_read(u, UC_ARM_REG_CPSR, &out->cpsr_after);
        if (err != UC_ERR_OK) {
            snprintf(out->end_reason, sizeof(out->end_reason), "uc_err");
            snprintf(out->err_detail, sizeof(out->err_detail), "%u:%s", (unsigned)err,
                     uc_strerror(err));
            out->ok = 0;
        } else if ((out->pc_after & ~1u) == (stop_addr & ~1u)) {
            snprintf(out->end_reason, sizeof(out->end_reason), "stop_at_base");
            out->ok = 1;
        } else {
            snprintf(out->end_reason, sizeof(out->end_reason), "insn_limit_or_yield");
            out->ok = 1;
        }
    }
    for (i = 0; i < 17; i++) uc_reg_write(u, regs_idx[i], &saved[i]);
    return (err == UC_ERR_OK) ? 1 : 0;
#else
    (void)uc;
    (void)start_pc;
    (void)stop_addr;
    (void)insn_limit;
    (void)abi;
    if (out) {
        memset(out, 0, sizeof(*out));
        snprintf(out->end_reason, sizeof(out->end_reason), "no_unicorn");
        snprintf(out->err_detail, sizeof(out->err_detail), "no_unicorn");
    }
    return 0;
#endif
}

int guest_memory_uc_run_bounded(struct uc_struct *uc, uint32_t start_pc, uint32_t stop_addr,
                                uint32_t r0_value, uint64_t insn_limit, char *err_buf,
                                size_t err_cap) {
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = r0_value;
    abi.set_lr = 1;
    abi.lr = stop_addr;
    ok = guest_memory_uc_run_entry_ex(uc, start_pc, stop_addr, insn_limit, &abi, &out);
    if (err_buf && err_cap) {
        if (!ok)
            snprintf(err_buf, err_cap, "%s", out.err_detail[0] ? out.err_detail : "uc_err");
        else
            err_buf[0] = 0;
    }
    return ok;
}

static uint64_t g_r9_write_seq;
static GwyR9WriteRecord g_r9_first_zero;
static GwyR9WriteRecord g_r9_last;

void guest_memory_r9_write_reset(void) {
    g_r9_write_seq = 0;
    memset(&g_r9_first_zero, 0, sizeof(g_r9_first_zero));
    memset(&g_r9_last, 0, sizeof(g_r9_last));
}

uint64_t guest_memory_r9_write_event_seq(void) { return g_r9_write_seq; }

const char *gwy_r9_write_reason_name(GwyR9WriteReason r) {
    switch (r) {
    case GWY_R9_WRITE_MODULE_R9_SWITCH_ENTER: return "MODULE_R9_SWITCH_ENTER";
    case GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE: return "MODULE_R9_SWITCH_LEAVE";
    case GWY_R9_WRITE_MODULE_R9_SWITCH_ABORT: return "MODULE_R9_SWITCH_ABORT";
    case GWY_R9_WRITE_GUEST_CONTEXT_RESET: return "GUEST_CONTEXT_RESET";
    case GWY_R9_WRITE_EMULATION_RESTART: return "EMULATION_RESTART";
    case GWY_R9_WRITE_HELPER_WRAPPER: return "HELPER_WRAPPER";
    case GWY_R9_WRITE_CALLBACK_RESUME: return "CALLBACK_RESUME";
    case GWY_R9_WRITE_BRIDGE_RAW_UC: return "BRIDGE_RAW_UC";
    default: return "OTHER";
    }
}

int guest_memory_r9_first_zeroing(GwyR9WriteRecord *out) {
    if (!out) return 0;
    if (!g_r9_first_zero.valid) return 0;
    *out = g_r9_first_zero;
    return 1;
}

int guest_memory_r9_last_write(GwyR9WriteRecord *out) {
    if (!out) return 0;
    if (!g_r9_last.valid) return 0;
    *out = g_r9_last;
    return 1;
}

int guest_memory_uc_read_r9(struct uc_struct *uc, uint32_t *out_r9) {
    uint32_t r9 = 0;
    if (!uc || !out_r9) return 0;
    if (uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9) != UC_ERR_OK) return 0;
    *out_r9 = r9;
    return 1;
}

int guest_memory_uc_write_r9(struct uc_struct *uc, uint32_t r9) {
    return guest_memory_uc_write_r9_ex(uc, r9, NULL);
}

int guest_memory_uc_write_r9_ex(struct uc_struct *uc, uint32_t r9, const GwyR9WriteAudit *audit) {
    uint32_t old_r9 = 0;
    GwyR9WriteRecord rec;
    GwyR9WriteReason reason = GWY_R9_WRITE_OTHER;
    if (!uc) return 0;
    (void)guest_memory_uc_read_r9(uc, &old_r9);
    if (uc_reg_write((uc_engine *)uc, UC_ARM_REG_R9, &r9) != UC_ERR_OK) return 0;

    g_r9_write_seq++;
    memset(&rec, 0, sizeof(rec));
    rec.valid = 1;
    rec.event_seq = g_r9_write_seq;
    rec.old_r9 = old_r9;
    rec.new_r9 = r9;
    if (audit) {
        reason = audit->reason;
        rec.frame_id = audit->frame_id;
        rec.scope_id = audit->scope_id;
        if (audit->host_callsite)
            snprintf(rec.host_callsite, sizeof(rec.host_callsite), "%s", audit->host_callsite);
        rec.guest_pc = audit->guest_pc;
        if (audit->guest_module)
            snprintf(rec.guest_module, sizeof(rec.guest_module), "%s", audit->guest_module);
        rec.depth_before = audit->depth_before;
        rec.depth_after = audit->depth_after;
    }
    rec.reason = reason;

    printf("[R9_WRITE] event_seq=%llu old=0x%X new=0x%X reason=%s frame_id=%llu scope_id=%llu "
           "host_callsite=%s guest_pc=0x%X guest_module=%s depth_before=%u depth_after=%u "
           "evidence=OBSERVED\n",
           (unsigned long long)rec.event_seq, rec.old_r9, rec.new_r9,
           gwy_r9_write_reason_name(rec.reason), (unsigned long long)rec.frame_id,
           (unsigned long long)rec.scope_id, rec.host_callsite[0] ? rec.host_callsite : "?",
           rec.guest_pc, rec.guest_module[0] ? rec.guest_module : "?", rec.depth_before,
           rec.depth_after);
    fflush(stdout);

    g_r9_last = rec;
    if (!g_r9_first_zero.valid && old_r9 != 0 && r9 == 0) g_r9_first_zero = rec;
    return 1;
}
