/* Stage E8A F6: Unicorn Thumb probe with per-insn T-bit ring. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/unicorn.h>

typedef struct {
    uint32_t pc;
    uint32_t size;
    uint32_t cpsr;
    uint8_t bytes[8];
} TraceInsn;

static TraceInsn g_ring[64];
static unsigned g_n;
static unsigned g_next;

static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    TraceInsn *e;
    uint32_t cpsr = 0;
    uint32_t sz = size;
    (void)user;
    if (sz > 8) sz = 8;
    e = &g_ring[g_next % 64];
    memset(e, 0, sizeof(*e));
    e->pc = (uint32_t)address;
    e->size = sz;
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    e->cpsr = cpsr;
    uc_mem_read(uc, address, e->bytes, sz);
    g_next++;
    if (g_n < 64) g_n++;
}

static int load_file(const char *path, uint8_t *buf, size_t cap, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, cap, f);
    fclose(f);
    if (out_len) *out_len = n;
    return n > 0;
}

int main(int argc, char **argv) {
    const char *ext_path = "out/forensics/jjfb_extracted/robotol.ext";
    uint32_t code_base = 0x2D8DF4u;
    uint32_t handler = 0x30630Cu;
    uint32_t stop = 0x306350u;
    uint32_t map_base, map_size;
    uint8_t file[512 * 1024];
    size_t file_len = 0;
    uc_engine *uc = NULL;
    uc_hook hh;
    uc_err err;
    uint32_t cpsr, pc, sp, lr, r0, r9;
    unsigned i, show, start;

    if (argc >= 2) ext_path = argv[1];
    if (argc >= 3) code_base = (uint32_t)strtoul(argv[2], NULL, 0);
    if (argc >= 4) handler = (uint32_t)strtoul(argv[3], NULL, 0);
    if (argc >= 5) stop = (uint32_t)strtoul(argv[4], NULL, 0);

    if (!load_file(ext_path, file, sizeof(file), &file_len)) {
        fprintf(stderr, "load failed: %s\n", ext_path);
        return 2;
    }

    map_base = handler & ~0xFFFu;
    map_size = 0x2000u;
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
    if (err) return 3;
    uc_mem_map(uc, map_base, map_size, UC_PROT_ALL);
    {
        uint32_t copy_off = map_base - code_base;
        size_t copy_len = map_size;
        if (copy_off + copy_len > file_len) copy_len = file_len - copy_off;
        uc_mem_write(uc, map_base, file + copy_off, copy_len);
    }
    /* Map a fake ER_RW page so LDR via R9 does not abort early. */
    uc_mem_map(uc, 0x2B1000u, 0x2000u, UC_PROT_ALL);
    uc_mem_map(uc, 0x27F000u, 0x2000u, UC_PROT_ALL);

    sp = 0x27FE00u;
    lr = 0x100000u;
    r0 = 0;
    r9 = 0x2B1858u;
    cpsr = (1u << 5);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    uc_reg_write(uc, UC_ARM_REG_R9, &r9);
    uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);

    uc_hook_add(uc, &hh, UC_HOOK_CODE, (void *)on_code, NULL, map_base,
                map_base + map_size - 1);

    printf("[UNICORN_PROBE] unicorn=1.0.2 handler=0x%X stop=0x%X entry_T=1\n", handler, stop);
    err = uc_emu_start(uc, handler, stop, 0, 128);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    printf("[UNICORN_PROBE] result err=%u (%s) pc=0x%X T=%d\n", (unsigned)err, uc_strerror(err),
           pc, (cpsr >> 5) & 1);

    show = g_n < 32 ? g_n : 32;
    start = (g_next + 64 - show) % 64;
    for (i = 0; i < show; i++) {
        TraceInsn *e = &g_ring[(start + i) % 64];
        unsigned j;
        printf("[PROBE_INSN] pc=0x%X size=%u T=%d bytes=", e->pc, e->size, (e->cpsr >> 5) & 1);
        for (j = 0; j < e->size; j++) printf("%02X", e->bytes[j]);
        printf("\n");
    }
    uc_close(uc);
    if (err == UC_ERR_OK) return 0;
    if (err == UC_ERR_INSN_INVALID) return 10;
    return 11;
}
