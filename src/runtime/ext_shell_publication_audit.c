#include "gwy_launcher/ext_shell_publication_audit.h"
#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t off;
    uint32_t old_v;
    uint32_t new_v;
    uint32_t pc;
    uint32_t lr;
    char module[48];
    int seen;
} PubField;

static struct {
    int enabled_known;
    int enabled;
    int finalized;
    void *uc;
    uint32_t p_guest;
    PubField fields[5]; /* 0,4,8,0xC,0x10 */
    int wrote_0;
    int wrote_4;
    int wrote_8;
    int wrote_c;
    int wrote_10;
    uint32_t last_048_pc;
    char last_048_module[48];
} g_pub;

static int env_is_1(const char *key) {
    const char *e = getenv(key);
    return e && e[0] == '1';
}

int ext_shell_publication_audit_enabled(void) {
    if (!g_pub.enabled_known) {
        g_pub.enabled = env_is_1("JJFB_PUBLICATION_AUDIT") ||
                        env_is_1("JJFB_MULTI_TARGET_MIN_COMPARE");
        g_pub.enabled_known = 1;
    }
    return g_pub.enabled;
}

void ext_shell_publication_audit_reset(void) {
    memset(&g_pub, 0, sizeof(g_pub));
}

void ext_shell_publication_audit_bind_uc(void *uc) { g_pub.uc = uc; }

void ext_shell_publication_audit_on_p_candidate(uint32_t p_guest) {
    if (!ext_shell_publication_audit_enabled()) return;
    if (p_guest) g_pub.p_guest = p_guest;
    ext_entry_abi_cluster_audit_on_p_candidate(p_guest, "publication");
}

static PubField *slot_for(uint32_t off) {
    int i;
    static const uint32_t kOffs[5] = {0x00u, 0x04u, 0x08u, 0x0Cu, 0x10u};
    for (i = 0; i < 5; i++) {
        if (kOffs[i] == off) return &g_pub.fields[i];
    }
    return NULL;
}

void ext_shell_publication_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                            uint32_t new_v, uint32_t pc, uint32_t lr,
                                            const char *module) {
    PubField *f;
    const char *mod;
    if (!ext_shell_publication_audit_enabled()) return;
    if (p_guest) g_pub.p_guest = p_guest;
    if (off != 0x00u && off != 0x04u && off != 0x08u && off != 0x0Cu && off != 0x10u) return;

    mod = module && module[0] ? module : "?";
    f = slot_for(off);
    if (f) {
        f->off = off;
        f->old_v = old_v;
        f->new_v = new_v;
        f->pc = pc;
        f->lr = lr;
        snprintf(f->module, sizeof(f->module), "%s", mod);
        f->seen = 1;
    }
    if (off == 0x00u) g_pub.wrote_0 = 1;
    if (off == 0x04u) g_pub.wrote_4 = 1;
    if (off == 0x08u) g_pub.wrote_8 = 1;
    if (off == 0x0Cu) g_pub.wrote_c = 1;
    if (off == 0x10u) g_pub.wrote_10 = 1;
    if (off == 0x00u || off == 0x04u || off == 0x08u) {
        g_pub.last_048_pc = pc;
        snprintf(g_pub.last_048_module, sizeof(g_pub.last_048_module), "%s", mod);
    }

    printf("[JJFB_P_FIELD_WRITE] off=0x%02X old=0x%X new=0x%X pc=0x%X lr=0x%X module=%s "
           "P=0x%X evidence=TARGET_OBSERVED\n",
           off, old_v, new_v, pc, lr, mod, g_pub.p_guest);
    printf("[JJFB_P_PROVIDER] write P+0x%02X pc=0x%X module=%s value=0x%X evidence=TARGET_OBSERVED\n",
           off, pc, mod, new_v);
    ext_entry_abi_cluster_audit_on_p_write(p_guest, off, old_v, new_v, pc, mod);
    ext_cfunction_publication_audit_on_p_write(p_guest, off, old_v, new_v, pc, lr, mod);
    /* Phase 6O: P+0/+4 → registry ER_RW + extChunk var (gated). */
    if (off == 0x00u || off == 0x04u)
        ext_er_rw_bind_restore_on_p_write(p_guest, off, new_v, mod);
    fflush(stdout);
}

void ext_shell_publication_audit_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr,
                                              uint32_t p_guest, uint32_t p_plus_c) {
    if (!ext_shell_publication_audit_enabled() || g_pub.finalized) return;
    if (p_guest) g_pub.p_guest = p_guest;

    printf("[JJFB_P_PROVIDER] chain P=0x%X wrote_0=%s wrote_4=%s wrote_8=%s wrote_C=%s "
           "wrote_10=%s P+0xC=0x%X fault_pc=0x%X fault_addr=0x%X evidence=TARGET_OBSERVED\n",
           g_pub.p_guest, g_pub.wrote_0 ? "yes" : "no", g_pub.wrote_4 ? "yes" : "no",
           g_pub.wrote_8 ? "yes" : "no", g_pub.wrote_c ? "yes" : "no",
           g_pub.wrote_10 ? "yes" : "no", p_plus_c, fault_pc, fault_addr);
    if (!g_pub.wrote_c && (g_pub.wrote_0 || g_pub.wrote_4 || g_pub.wrote_8)) {
        printf("[JJFB_P_PROVIDER] missing P+0x0C nearest_048_writer_pc=0x%X module=%s "
               "note=same_init_path_skipped_extchunk evidence=TARGET_OBSERVED\n",
               g_pub.last_048_pc,
               g_pub.last_048_module[0] ? g_pub.last_048_module : "?");
    } else if (!g_pub.wrote_c) {
        printf("[JJFB_P_PROVIDER] missing P+0x0C nearest_048_writer_pc=0x0 module=none "
               "note=no_048_writes_seen evidence=TARGET_OBSERVED\n");
    }
    fflush(stdout);
    ext_shell_publication_audit_finalize("mem_fault");
}

void ext_shell_publication_audit_finalize(const char *stop_reason) {
    if (!ext_shell_publication_audit_enabled() || g_pub.finalized) return;
    g_pub.finalized = 1;
    printf("[JJFB_PUBLICATION_SUMMARY] P=0x%X wrote_0=%s wrote_4=%s wrote_8=%s wrote_C=%s "
           "wrote_10=%s last_048_pc=0x%X last_048_module=%s stop=%s evidence=TARGET_OBSERVED\n",
           g_pub.p_guest, g_pub.wrote_0 ? "yes" : "no", g_pub.wrote_4 ? "yes" : "no",
           g_pub.wrote_8 ? "yes" : "no", g_pub.wrote_c ? "yes" : "no",
           g_pub.wrote_10 ? "yes" : "no", g_pub.last_048_pc,
           g_pub.last_048_module[0] ? g_pub.last_048_module : "?",
           stop_reason ? stop_reason : "?");
    fflush(stdout);
}
