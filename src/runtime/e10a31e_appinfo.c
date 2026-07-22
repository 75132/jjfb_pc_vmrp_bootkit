#include "gwy_launcher/e10a31e_appinfo.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/package_scope.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10A31E_READ_MAX 128

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t addr;
    uint32_t value;
    char field[32];
    char enclosing[48];
    char branch_rel[64];
} ReadRow;

static struct {
    int known;
    int enabled;
    int metadata;
    int binding;
    int ab;
    int method0;
    int read_proof;
    unsigned long long run_id;

    FILE *meta_csv;
    FILE *bind_csv;
    FILE *owner_csv;
    FILE *globals_csv;
    FILE *ab_csv;
    FILE *read_csv;

    uint32_t last_appinfo;
    uint32_t last_appid;
    uint32_t last_appver;
    uint32_t last_erw;
    int32_t last_ret6;
    int32_t last_ret8;
    int32_t last_ret0;
    uint32_t first_fail_pc;
    char first_fail_class[48];

#ifdef GWY_HAVE_UNICORN
    uc_hook read_hook;
    int read_armed;
#endif
    uint32_t watch_erw;
    uint32_t watch_appinfo;
    ReadRow reads[E10A31E_READ_MAX];
    uint32_t read_n;
    int saw_id_read;
    int saw_ver_read;
} g_e;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31E_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A31D_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *hdr) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "w");
    if (!f) return NULL;
    fputs(hdr, f);
    fflush(f);
    return f;
}

static void ensure_enabled(void) {
    if (g_e.known) return;
    g_e.known = 1;
    g_e.enabled = env1("JJFB_E10A31E_MODE") || env1("JJFB_E10A31E_METADATA") ||
                  env1("JJFB_E10A31E_BINDING") || env1("JJFB_E10A31E_AB_COMPARE") ||
                  env1("JJFB_E10A31E_METHOD0") || env1("JJFB_E10A31E_READ_PROOF");
    g_e.metadata = env1("JJFB_E10A31E_METADATA") || env1("JJFB_E10A31E_MODE");
    g_e.binding = env1("JJFB_E10A31E_BINDING") || env1("JJFB_E10A31E_MODE");
    g_e.ab = env1("JJFB_E10A31E_AB_COMPARE");
    g_e.method0 = env1("JJFB_E10A31E_METHOD0") || env1("JJFB_E10A31E_MODE");
    g_e.read_proof = env1("JJFB_E10A31E_READ_PROOF");
    g_e.run_id = run_id_now();
    g_e.last_ret6 = g_e.last_ret8 = g_e.last_ret0 = 0x7fffffff;
}

int e10a31e_enabled(void) {
    ensure_enabled();
    return g_e.enabled;
}

void e10a31e_reset(void) {
#ifdef GWY_HAVE_UNICORN
    g_e.read_armed = 0;
    g_e.read_hook = 0;
#endif
    if (g_e.meta_csv) fclose(g_e.meta_csv);
    if (g_e.bind_csv) fclose(g_e.bind_csv);
    if (g_e.owner_csv) fclose(g_e.owner_csv);
    if (g_e.globals_csv) fclose(g_e.globals_csv);
    if (g_e.ab_csv) fclose(g_e.ab_csv);
    if (g_e.read_csv) fclose(g_e.read_csv);
    memset(&g_e, 0, sizeof(g_e));
}

void e10a31e_mark_milestone(const char *name, const char *note) {
    ensure_enabled();
    if (!g_e.enabled || !name) return;
    printf("[JJFB_E10A31E_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

static uint32_t peek_u32(void *uc, uint32_t addr, int *ok) {
    uint32_t v = 0;
    *ok = 0;
    if (!uc || !addr) return 0;
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v)) {
        *ok = 1;
        return v;
    }
    return 0;
}

void e10a31e_note_metadata(const char *phase) {
    const GwyPackageMetadata *m;
    ensure_enabled();
    if (!g_e.enabled || !g_e.metadata) return;
    m = gwy_package_registry_active_metadata();
    if (!g_e.meta_csv) {
        g_e.meta_csv = open_csv(
            "JJFB_E10A31E_META_CSV", "reports/e10a31e_package_metadata_trace.csv",
            "run_id,phase,package,package_id,generation,archive,source,appid,appver,"
            "internal_name,valid\n");
    }
    if (g_e.meta_csv) {
        if (m && m->valid) {
            fprintf(g_e.meta_csv,
                    "%llu,\"%s\",\"%s\",%llu,%llu,\"%s\",\"%s\",%u,%u,\"%s\",%d\n", g_e.run_id,
                    phase ? phase : "?", m->guest_path, (unsigned long long)m->package_id,
                    (unsigned long long)m->package_generation, m->archive_path,
                    gwy_package_metadata_source_name(m->source), m->appid, m->appver,
                    m->internal_name, m->valid);
        } else {
            fprintf(g_e.meta_csv, "%llu,\"%s\",,,0,0,,,0,0,,0\n", g_e.run_id,
                    phase ? phase : "?");
        }
        fflush(g_e.meta_csv);
    }
    if (m && m->valid)
        e10a31e_mark_milestone("ACTIVE_PACKAGE_METADATA_RESOLVED", m->guest_path);
    else
        e10a31e_mark_milestone("ACTIVE_PACKAGE_METADATA_BINDING_BROKEN", phase ? phase : "?");
}

void e10a31e_note_binding(uint32_t guest_ptr, uint32_t appid, uint32_t appver, int source) {
    const GwyPackageMetadata *m;
    const GwyPackageAppInfoBinding *b;
    ensure_enabled();
    if (!g_e.enabled) return;
    m = gwy_package_registry_active_metadata();
    b = gwy_package_appinfo_binding_active();
    g_e.last_appinfo = guest_ptr;
    g_e.last_appid = appid;
    g_e.last_appver = appver;

    printf("[GWY_APPINFO_BIND] package=%s package_id=%llu generation=%llu archive=%s "
           "source=%s appid=%u appver=%u guest_ptr=0x%X sidName=0x0 ram=0 evidence=OBSERVED\n",
           m && m->valid ? m->guest_path : "?",
           m ? (unsigned long long)m->package_id : 0ull,
           m ? (unsigned long long)m->package_generation : 0ull,
           m && m->valid ? m->archive_path : "?", gwy_package_metadata_source_name(source), appid,
           appver, guest_ptr);
    fflush(stdout);

    if (!g_e.binding) return;
    if (!g_e.bind_csv) {
        g_e.bind_csv = open_csv(
            "JJFB_E10A31E_BIND_CSV", "reports/e10a31e_appinfo_binding_trace.csv",
            "run_id,package,package_id,generation,archive,source,appid,appver,guest_ptr,"
            "sidName,ram,binding_valid\n");
    }
    if (g_e.bind_csv) {
        fprintf(g_e.bind_csv,
                "%llu,\"%s\",%llu,%llu,\"%s\",\"%s\",%u,%u,0x%X,0x0,0,%d\n", g_e.run_id,
                m && m->valid ? m->guest_path : "?", m ? (unsigned long long)m->package_id : 0ull,
                m ? (unsigned long long)m->package_generation : 0ull,
                m && m->valid ? m->archive_path : "?", gwy_package_metadata_source_name(source),
                appid, appver, guest_ptr, b && b->valid ? 1 : 0);
        fflush(g_e.bind_csv);
    }
    e10a31e_mark_milestone("APPINFO_BOUND_TO_ACTIVE_PACKAGE_METADATA",
                           gwy_package_metadata_source_name(source));
}

static int path_is_gamelist(const char *p) {
    return p && (strstr(p, "gamelist.mrp") != NULL || strstr(p, "gamelist") != NULL);
}

void e10a31e_before_code8(void *uc, uint32_t helper, uint32_t erw, uint32_t appinfo) {
    const GwyPackageMetadata *m;
    const GwyPackageAppInfoBinding *b;
    const char *active_pkg;
    const char *active_pri;
    ModuleRegistry *reg;
    const GwyLoadedModule *owner = NULL;
    const char *verdict = "APPINFO_METADATA_NOT_AVAILABLE";
    uint64_t helper_pid = 0, helper_gen = 0;
    int r9_ok = 0;
    (void)uc;
    (void)erw;

    ensure_enabled();
    if (!g_e.enabled) return;

    m = gwy_package_registry_active_metadata();
    b = gwy_package_appinfo_binding_active();
    active_pkg = package_scope_active_package();
    active_pri = package_scope_active_primary();
    reg = gwy_ext_loader_bound_registry();
    if (reg && helper) owner = module_registry_find_by_helper(reg, helper);

    if (!m || !m->valid) {
        verdict = "APPINFO_METADATA_NOT_AVAILABLE";
    } else if (b && b->valid && b->package_generation != m->package_generation) {
        verdict = "APPINFO_OWNER_STALE_GENERATION";
    } else if (!path_is_gamelist(active_pkg) || !path_is_gamelist(m->guest_path) ||
               !path_is_gamelist(m->archive_path)) {
        verdict = "APPINFO_OWNER_WRONG_PACKAGE";
    } else if (active_pri && strstr(active_pri, "gamelist") == NULL) {
        verdict = "APPINFO_OWNER_WRONG_PACKAGE";
    } else if (owner) {
        helper_pid = owner->module_id;
        /* Module id is not package_id; compare archive ownership by path. */
        if (!path_is_gamelist(owner->package_path) &&
            !path_is_gamelist(owner->resolved_name) &&
            !path_is_gamelist(owner->requested_name)) {
            verdict = "APPINFO_OWNER_WRONG_PACKAGE";
        } else if (b && b->valid && appinfo && b->appinfo_guest != appinfo) {
            verdict = "APPINFO_OWNER_WRONG_PACKAGE";
        } else {
            verdict = "APPINFO_OWNER_MATCH";
            r9_ok = 1;
            (void)helper_gen;
        }
    } else if (path_is_gamelist(active_pkg) && path_is_gamelist(m->guest_path)) {
        verdict = "APPINFO_OWNER_MATCH";
        r9_ok = 1;
    }

    printf("[JJFB_E10A31E_OWNER] verdict=%s active_pkg=%s primary=%s meta_pkg=%s "
           "helper=0x%X helper_module=%llu appinfo=0x%X meta_id=%llu meta_gen=%llu "
           "r9_owner_ok=%d evidence=OBSERVED\n",
           verdict, active_pkg ? active_pkg : "?", active_pri ? active_pri : "?",
           m && m->valid ? m->guest_path : "?", helper, (unsigned long long)helper_pid, appinfo,
           m ? (unsigned long long)m->package_id : 0ull,
           m ? (unsigned long long)m->package_generation : 0ull, r9_ok);
    fflush(stdout);

    if (!g_e.owner_csv) {
        g_e.owner_csv = open_csv(
            "JJFB_E10A31E_OWNER_CSV", "reports/e10a31e_appinfo_owner_validation.csv",
            "run_id,verdict,active_package,active_primary,meta_package,meta_archive,"
            "helper,helper_module_id,appinfo,meta_package_id,meta_generation\n");
    }
    if (g_e.owner_csv) {
        fprintf(g_e.owner_csv,
                "%llu,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",0x%X,%llu,0x%X,%llu,%llu\n",
                g_e.run_id, verdict, active_pkg ? active_pkg : "",
                active_pri ? active_pri : "", m && m->valid ? m->guest_path : "",
                m && m->valid ? m->archive_path : "", helper, (unsigned long long)helper_pid,
                appinfo, m ? (unsigned long long)m->package_id : 0ull,
                m ? (unsigned long long)m->package_generation : 0ull);
        fflush(g_e.owner_csv);
    }
    e10a31e_mark_milestone(verdict, active_pkg ? active_pkg : "?");
}

void e10a31e_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret) {
    int ok = 0;
    uint32_t v91c;
    ensure_enabled();
    if (!g_e.enabled) return;
    g_e.last_erw = erw;
    g_e.last_ret6 = ret;
    v91c = peek_u32(uc, erw + 0x91Cu, &ok);
    printf("[JJFB_E10A31E_GLOBALS] phase=code6 R9+0x91C=0x%X expected=%u ret=%d "
           "evidence=OBSERVED\n",
           v91c, input_len, (int)ret);
    fflush(stdout);
    if (!g_e.globals_csv) {
        g_e.globals_csv =
            open_csv("JJFB_E10A31E_GLOBALS_CSV", "reports/e10a31e_gamelist_globals.csv",
                     "run_id,phase,erw,slot,value,expected,appinfo,id,ver,sid,ram,ret,note\n");
    }
    if (g_e.globals_csv) {
        fprintf(g_e.globals_csv, "%llu,code6,0x%X,0x91C,0x%X,%u,0,0,0,0,0,%d,MR_VERSION\n",
                g_e.run_id, erw, v91c, input_len, (int)ret);
        fflush(g_e.globals_csv);
    }
    if (ok && v91c == input_len)
        e10a31e_mark_milestone("GAMELIST_MR_VERSION_PUBLISHED", "R9+0x91C");
}

void e10a31e_after_code8(void *uc, uint32_t erw, uint32_t appinfo, int32_t ret) {
    int ok = 0, okb = 0;
    uint32_t v920, id = 0, ver = 0, sid = 0, ram = 0;
    uint8_t raw[16];
    const GwyPackageMetadata *m;
    ensure_enabled();
    if (!g_e.enabled) return;
    g_e.last_erw = erw;
    g_e.last_ret8 = ret;
    g_e.last_appinfo = appinfo;
    v920 = peek_u32(uc, erw + 0x920u, &ok);
    memset(raw, 0, sizeof(raw));
    if (appinfo && guest_memory_uc_peek((struct uc_struct *)uc, appinfo, raw, 16)) {
        okb = 1;
        id = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) |
             ((uint32_t)raw[3] << 24);
        ver = (uint32_t)raw[4] | ((uint32_t)raw[5] << 8) | ((uint32_t)raw[6] << 16) |
              ((uint32_t)raw[7] << 24);
        sid = (uint32_t)raw[8] | ((uint32_t)raw[9] << 8) | ((uint32_t)raw[10] << 16) |
              ((uint32_t)raw[11] << 24);
        ram = (uint32_t)raw[12] | ((uint32_t)raw[13] << 8) | ((uint32_t)raw[14] << 16) |
              ((uint32_t)raw[15] << 24);
        g_e.last_appid = id;
        g_e.last_appver = ver;
    }
    m = gwy_package_registry_active_metadata();
    printf("[JJFB_E10A31E_GLOBALS] phase=code8 R9+0x920=0x%X appinfo=0x%X id=%u ver=%u "
           "sid=0x%X ram=%u ret=%d evidence=OBSERVED\n",
           v920, appinfo, id, ver, sid, ram, (int)ret);
    fflush(stdout);
    if (!g_e.globals_csv) {
        g_e.globals_csv =
            open_csv("JJFB_E10A31E_GLOBALS_CSV", "reports/e10a31e_gamelist_globals.csv",
                     "run_id,phase,erw,slot,value,expected,appinfo,id,ver,sid,ram,ret,note\n");
    }
    if (g_e.globals_csv) {
        fprintf(g_e.globals_csv,
                "%llu,code8,0x%X,0x920,0x%X,0x%X,0x%X,%u,%u,0x%X,%u,%d,appInfo\n", g_e.run_id,
                erw, v920, appinfo, appinfo, id, ver, sid, ram, (int)ret);
        fflush(g_e.globals_csv);
    }
    if (ok && v920 == appinfo && appinfo)
        e10a31e_mark_milestone("GAMELIST_APPINFO_PUBLISHED", "R9+0x920");
    if (m && m->valid && okb && id == m->appid && ver == m->appver)
        e10a31e_mark_milestone("APPINFO_PACKAGE_METADATA_MATCH", "id_ver");
}

void e10a31e_after_method0(void *uc, uint32_t helper, int32_t ret) {
    (void)uc;
    (void)helper;
    ensure_enabled();
    if (!g_e.enabled) return;
    g_e.last_ret0 = ret;
    if (ret == 0) {
        e10a31e_mark_milestone("GAMELIST_METHOD0_RETURN_ZERO", "ret0");
        e10a31e_mark_milestone("MRC_INIT_ACCEPTS_PACKAGE_METADATA", "ret0");
        e10a31e_mark_milestone("GAMELIST_INIT_SEQUENCE_COMPLETE", "6_8_0");
    } else if (ret < 0) {
        e10a31e_mark_milestone("GAMELIST_INIT_METHOD0_FAILED", "ret_neg1");
        /* Outcome 2 vs 3 decided from e10a31d provenance fail PC in runner. */
    }
}

void e10a31e_note_ab_case(const char *case_name, int32_t ret6, int32_t ret8, int32_t ret0,
                          uint32_t appid, uint32_t appver, uint32_t fail_pc,
                          const char *fail_class) {
    ensure_enabled();
    if (!g_e.enabled) return;
    if (!g_e.ab_csv) {
        const char *p = getenv("JJFB_E10A31E_AB_CSV");
        if (!p || !p[0]) p = "reports/e10a31e_appinfo_ab_compare.csv";
        /* Append across A/B process runs; write header only if new/empty. */
        {
            FILE *chk = fopen(p, "r");
            int need_hdr = 1;
            if (chk) {
                need_hdr = 0;
                fclose(chk);
            }
            g_e.ab_csv = fopen(p, need_hdr ? "w" : "a");
            if (g_e.ab_csv && need_hdr) {
                fputs("run_id,case,appid,appver,ret6,ret8,ret0,fail_pc,fail_class,"
                      "platform_api_count,cfg_gate\n",
                      g_e.ab_csv);
                fflush(g_e.ab_csv);
            }
        }
    }
    if (g_e.ab_csv) {
        fprintf(g_e.ab_csv, "%llu,\"%s\",%u,%u,%d,%d,%d,0x%X,\"%s\",0,0\n", g_e.run_id,
                case_name ? case_name : "?", appid, appver, (int)ret6, (int)ret8, (int)ret0,
                fail_pc, fail_class ? fail_class : "");
        fflush(g_e.ab_csv);
    }
    printf("[JJFB_E10A31E_AB] case=%s appid=%u appver=%u ret6=%d ret8=%d ret0=%d "
           "fail_pc=0x%X class=%s evidence=OBSERVED\n",
           case_name ? case_name : "?", appid, appver, (int)ret6, (int)ret8, (int)ret0, fail_pc,
           fail_class ? fail_class : "");
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void classify_read(uint32_t addr, char *field, size_t field_sz, char *encl, size_t encl_sz,
                          char *rel, size_t rel_sz) {
    uint32_t slot920 = g_e.watch_erw ? g_e.watch_erw + 0x920u : 0;
    snprintf(encl, encl_sz, "method0");
    snprintf(rel, rel_sz, "pre_2E1C24");
    if (slot920 && addr >= slot920 && addr < slot920 + 4u) {
        snprintf(field, field_sz, "R9+0x920");
        return;
    }
    if (g_e.watch_appinfo) {
        if (addr >= g_e.watch_appinfo && addr < g_e.watch_appinfo + 4u) {
            snprintf(field, field_sz, "appInfo.id");
            g_e.saw_id_read = 1;
            return;
        }
        if (addr >= g_e.watch_appinfo + 4u && addr < g_e.watch_appinfo + 8u) {
            snprintf(field, field_sz, "appInfo.ver");
            g_e.saw_ver_read = 1;
            return;
        }
        if (addr >= g_e.watch_appinfo + 8u && addr < g_e.watch_appinfo + 12u) {
            snprintf(field, field_sz, "appInfo.sidName");
            return;
        }
        if (addr >= g_e.watch_appinfo + 12u && addr < g_e.watch_appinfo + 16u) {
            snprintf(field, field_sz, "appInfo.ram");
            return;
        }
    }
    snprintf(field, field_sz, "other");
}

static void e10a31e_on_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                                int64_t value, void *user_data) {
    uint32_t pc = 0, addr = (uint32_t)address, v = 0;
    char field[32], encl[48], rel[64];
    int interesting = 0;
    uint32_t slot920;
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_e.read_proof || !g_e.read_armed) return;
    slot920 = g_e.watch_erw ? g_e.watch_erw + 0x920u : 0;
    if (slot920 && addr >= slot920 && addr < slot920 + 4u) interesting = 1;
    if (g_e.watch_appinfo && addr >= g_e.watch_appinfo && addr < g_e.watch_appinfo + 16u)
        interesting = 1;
    /* Also catch pointer-target load of the stored appInfo* if R9+0x920 was read. */
    if (!interesting) return;

    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (size == 4)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v);
    else
        v = (uint32_t)(uint64_t)value;

    classify_read(addr, field, sizeof(field), encl, sizeof(encl), rel, sizeof(rel));
    if (g_e.read_n < E10A31E_READ_MAX) {
        ReadRow *r = &g_e.reads[g_e.read_n++];
        memset(r, 0, sizeof(*r));
        r->seq = g_e.read_n;
        r->pc = pc;
        r->addr = addr;
        r->value = v;
        snprintf(r->field, sizeof(r->field), "%s", field);
        snprintf(r->enclosing, sizeof(r->enclosing), "%s", encl);
        snprintf(r->branch_rel, sizeof(r->branch_rel), "%s", rel);
    }
    if (!g_e.read_csv) {
        g_e.read_csv = open_csv(
            "JJFB_E10A31E_READ_CSV", "reports/e10a31e_appinfo_read_proof.csv",
            "run_id,seq,pc,addr,value,field,enclosing_function,branch_relation\n");
    }
    if (g_e.read_csv) {
        fprintf(g_e.read_csv, "%llu,%u,0x%X,0x%X,0x%X,\"%s\",\"%s\",\"%s\"\n", g_e.run_id,
                g_e.read_n, pc, addr, v, field, encl, rel);
        fflush(g_e.read_csv);
    }
    printf("[JJFB_E10A31E_READ] field=%s addr=0x%X val=0x%X pc=0x%X evidence=OBSERVED\n", field,
           addr, v, pc);
    fflush(stdout);
    e10a31e_mark_milestone("APPINFO_READ_HOOK_FIXED", field);
    if (g_e.saw_id_read) e10a31e_mark_milestone("METHOD0_READS_APPINFO_ID", "read");
    if (g_e.saw_ver_read) e10a31e_mark_milestone("METHOD0_READS_APPINFO_VERSION", "read");
}
#endif

void e10a31e_read_proof_arm(void *uc, uint32_t erw, uint32_t appinfo) {
    ensure_enabled();
    if (!g_e.enabled || !g_e.read_proof || !uc) return;
    g_e.watch_erw = erw;
    g_e.watch_appinfo = appinfo;
    g_e.read_n = 0;
    g_e.saw_id_read = 0;
    g_e.saw_ver_read = 0;
#ifdef GWY_HAVE_UNICORN
    if (!g_e.read_armed) {
        /* Broad hook; filter in callback (R9+0x920 + appInfo 16B). */
        if (uc_hook_add((uc_engine *)uc, &g_e.read_hook, UC_HOOK_MEM_READ,
                        (void *)e10a31e_on_mem_read, NULL, 1, 0) == UC_ERR_OK)
            g_e.read_armed = 1;
    }
#else
    (void)uc;
#endif
    printf("[JJFB_E10A31E] read_proof=arm erw=0x%X appinfo=0x%X evidence=DOCUMENTED\n", erw,
           appinfo);
    fflush(stdout);
}

void e10a31e_read_proof_disarm(void *uc) {
    ensure_enabled();
#ifdef GWY_HAVE_UNICORN
    if (g_e.read_armed && uc) {
        uc_hook_del((uc_engine *)uc, g_e.read_hook);
        g_e.read_armed = 0;
        g_e.read_hook = 0;
    }
#else
    (void)uc;
#endif
    if (g_e.read_proof && g_e.read_n == 0)
        e10a31e_mark_milestone("METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE", "zero_reads");
}
