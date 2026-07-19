#include "./header/vmrp.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./header/bridge.h"
#include "./header/fileLib.h"
#include "./header/memory.h"
#include "./header/utils.h"
#include "./header/debug.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static uint8_t *mrpMem;  // 模拟器的全部内存
static uc_engine *uc = NULL;

static int jjfb_env_true(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return 0;
    return strcmp(v, "0") != 0 && strcmp(v, "off") != 0 &&
           strcmp(v, "false") != 0 && strcmp(v, "FALSE") != 0;
}

static uint32_t jjfb_be32(const uint8_t b[4]) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static int jjfb_read_mrp_appinfo(const char *guest_path, int32_t *app_id, int32_t *app_ver) {
    char host_path[JJFB_PATH_MAX];
    uint8_t buf[8];
    FILE *fp;
    if (!guest_path || !app_id || !app_ver) return 0;
    my_resolve_path(guest_path, host_path, sizeof(host_path));
    fp = fopen(host_path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 192, SEEK_SET) != 0 || fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *app_id = (int32_t)jjfb_be32(buf);
    *app_ver = (int32_t)jjfb_be32(buf + 4);
    return 1;
}

// 返回的内存禁止free
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void *getMrpMemPtr(uint32_t addr) {
    return mrpMem + (addr - START_ADDRESS);
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t toMrpMemAddr(void *ptr) {
    return ((uint8_t *)ptr - mrpMem) + START_ADDRESS;
}

#ifdef DEBUG
static void hook_block(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    printf(">>> Tracing basic block at 0x%" PRIx64 ", block size = 0x%x\n", address, size);
}
static void hook_mem_valid(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    printf(">>> Tracing mem_valid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
           memTypeStr(type), address, size, value);
    if (type == UC_MEM_READ && size <= 4) {
        uint32_t v, pc;
        uc_mem_read(uc, address, &v, size);
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        printf("PC:0x%X,read:0x%X\n", pc, v);
    }
}

#endif

static bool hook_mem_invalid(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    printf(">>> Tracing mem_invalid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
           memTypeStr(type), address, size, value);
    dumpREG(uc);
    return false;
}

int freeVmrp(uc_engine *uc) {
    free(mrpMem);
    uc_close(uc);
    return 0;
}

uc_engine *initVmrp() {
    uc_engine *uc;
    uc_err err;
    uc_hook trace;

    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
    if (err) {
        printf("Failed on uc_open() with error returned: %u (%s)\n", err, uc_strerror(err));
        return NULL;
    }

    mrpMem = malloc(TOTAL_MEMORY);
    // unicorn存在BUG，UC_HOOK_MEM_INVALID只能拦截第一次UC_MEM_FETCH_PROT，所以干脆设置成可执行，统一在UC_HOOK_CODE事件中处理
    err = uc_mem_map_ptr(uc, START_ADDRESS, TOTAL_MEMORY, UC_PROT_ALL, mrpMem);
    if (err) {
        printf("Failed mem map: %u (%s)\n", err, uc_strerror(err));
        goto end;
    }
    initMemoryManager(MEMORY_MANAGER_ADDRESS, MEMORY_MANAGER_SIZE);

    err = bridge_init(uc);
    if (err) {
        printf("Failed bridge_init(): %u (%s)\n", err, uc_strerror(err));
        goto end;
    }

#if 1
    // 一些游戏检测内存时会去读写0-CODE_ADDRESS地址的值, 不清楚为什么要这样做，如果这块内存没有映射到unicorn会因为无效的内存访问导致程序退出
    // 映射一块内存后那些无法运行的游戏就能打开了，没有深入去研究，先记录在这里
    // JJFB/robotol.ext hits UC_MEM_READ_UNMAPPED at 0x28 without this.
    char *ppp = malloc(CODE_ADDRESS);
    // memset(ppp, 0xFF, CODE_ADDRESS);
    memset(ppp, 0, CODE_ADDRESS);
    uc_mem_map_ptr(uc, 0, CODE_ADDRESS, UC_PROT_ALL, ppp);

    // uc_mem_map(uc, 0, CODE_ADDRESS, UC_PROT_ALL);
    // uc_hook_add(uc, &trace, UC_HOOK_MEM_VALID, hook_mem_valid, NULL, 0, CODE_ADDRESS);
#endif

#ifdef DEBUG
    uc_hook_add(uc, &trace, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);
    uc_hook_add(uc, &trace, UC_HOOK_MEM_VALID, hook_mem_valid, NULL, 1, 0);
    uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code_debug, NULL, 1, 0);
#endif
    uc_hook_add(uc, &trace, UC_HOOK_MEM_INVALID, hook_mem_invalid, NULL, 1, 0, 0);

    // 设置栈
    uint32_t value = STACK_ADDRESS + STACK_SIZE;  // 满递减
    uc_reg_write(uc, UC_ARM_REG_SP, &value);

    return uc;
end:
    uc_close(uc);
    return NULL;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
int32_t c_event(int32_t code, int32_t p1, int32_t p2) {
    if (uc) {
        return bridge_dsm_mr_event(uc, code, p1, p2);
    }
    return MR_FAILED;
}
#endif

int32_t event(int32_t code, int32_t p1, int32_t p2) {
    if (uc) {
        return bridge_dsm_mr_event(uc, code, p1, p2);
    }
    return MR_FAILED;
}

int32_t timer() {
    if (uc) {
        /* Prefer robotol EXT timer (code=2) once mrc_init armed it; avoids DSM
         * "mr_timer event unexpected!" when Lua timer state is still IDLE.
         * 0x10140 handler is invoked inside bridge_dsm_ext_call (mutex held). */
        if (jjfb_robotol_timer_active()) {
            return bridge_dsm_ext_call(uc, 2, NULL, 0);
        }
        return bridge_dsm_mr_timer(uc);
    }
    return MR_FAILED;
}

int32_t loadCode() {
    char *filename = "cfunction.ext";
    int32_t len = my_getLen(filename);
    char *buf = readFile(filename);
    if (buf == NULL) {
        return MR_FAILED;
    }
    uc_mem_write(uc, CODE_ADDRESS, buf, len);
    return MR_SUCCESS;
}

int startVmrp() {
    uc = initVmrp();
    if (uc == NULL) {
        printf("initVmrp() fail.\n");
        return MR_FAILED;
    }

    if (loadCode() == MR_FAILED) {
        printf("loadCode fail.\n");
        return MR_FAILED;
    }
    bridge_ext_init(uc);

    if (bridge_dsm_init(uc) == MR_SUCCESS) {
        printf("bridge_dsm_init success\n");
        dumpREG(uc);

        int gwy_launcher_mode = jjfb_env_true("JJFB_GWY_LAUNCHER_MODE");
        int member_alias_mode = jjfb_env_true("JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL");
        int start_ignore_handoff_mode = jjfb_env_true("JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL");
        char *filename = gwy_launcher_mode ? "gwy/jjfb.mrp" : "dsm_gm.mrp";
        // char *filename = "winmine.mrp";
        char *extName = "start.mr";
        // char *extName = "cfunction.ext";

        char *startParam = getenv("JJFB_GWY_PARAM");
        if (!startParam || !*startParam) {
            startParam = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
        }
        if (gwy_launcher_mode) {
            const char *mythroad_root = getenv("JJFB_MYTHROAD_ROOT");
            const char *gwy_root = getenv("JJFB_GWY_ROOT");
            char target_host[JJFB_PATH_MAX];
            my_resolve_path("gwy/jjfb.mrp", target_host, sizeof(target_host));
            printf("[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp\n");
            printf("[JJFB_GWY_LAUNCH] param=%s\n", startParam);
            printf("[JJFB_GWY_ROOT] mythroad_root=%s\n", mythroad_root ? mythroad_root : "(unset)");
            printf("[JJFB_GWY_ROOT] gwy_root=%s\n", gwy_root ? gwy_root : "(unset)");
            printf("[JJFB_GWY_ROOT] target=%s\n", target_host);
            printf("[JJFB_GWY_ROOT] resource_dirs=gifs,jjfbol,save,sound,...\n");
            printf("[JJFB_STARTGAME] startGame/runapp equivalent=bridge_dsm_mr_start_dsm\n");
            printf("[JJFB_GWY_LAUNCH] startGame/runapp equivalent called\n");
            printf("[JJFB_MRP_ALIAS] enabled=%d request=cfunction.ext target=robotol.ext\n",
                   member_alias_mode);
            fflush(stdout);
        }
        uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);
        printf("bridge_dsm_mr_start_dsm('%s','%s',SRCV): 0x%X\n", filename, extName, ret);

        /* The original start.mr executes robotol's internal 801 code=0.  On this
         * platform that path returns MR_IGNORE (1) after robotol is already loaded,
         * because appInfo/mrc_extChunk are not yet supplied.  Do not globally coerce
         * the return value: accept it only when the alias and robotol postconditions
         * prove that the second-stage EXT registration completed, then recover the
         * platform contract from host with 6 -> 8 -> 0. */
        int start_handoff_ready = (ret == MR_SUCCESS);
        if (!start_handoff_ready && start_ignore_handoff_mode &&
            ret == MR_IGNORE && member_alias_mode &&
            jjfb_mrp_alias_applied() && jjfb_robotol_ext_loaded()) {
            start_handoff_ready = 1;
            printf("[JJFB_START_HANDOFF] raw_ret=0x%X semantic=MR_IGNORE "
                   "alias_applied=1 robotol_loaded=1 helper=0x%X "
                   "action=run_host_801_recovery\n",
                   ret, jjfb_robotol_ext_helper());
            fflush(stdout);
        } else if (ret != MR_SUCCESS) {
            printf("[JJFB_START_HANDOFF] raw_ret=0x%X alias_applied=%d "
                   "robotol_loaded=%d recovery_enabled=%d action=stop_before_host_801\n",
                   ret, jjfb_mrp_alias_applied(), jjfb_robotol_ext_loaded(),
                   start_ignore_handoff_mode);
            fflush(stdout);
        }

        if (start_handoff_ready) {
            /* v51 could call 801 while mr_extHelper_addr still pointed at the
             * 232-byte mrc_loader.  In alias mode, require the post-alias EXT
             * registration before treating code=0 as robotol mrc_init. */
            if (member_alias_mode && !jjfb_robotol_ext_loaded()) {
                printf("[JJFB_801_GUARD] robotol_loaded=0 alias_applied=%d "
                       "current_helper=0x%X action=skip_host_801\n",
                       jjfb_mrp_alias_applied(), jjfb_current_ext_helper());
                fflush(stdout);
            } else {
                if (member_alias_mode) {
                    printf("[JJFB_801_GUARD] robotol_loaded=1 alias_applied=%d "
                           "robotol_helper=0x%X action=run_host_801\n",
                           jjfb_mrp_alias_applied(), jjfb_robotol_ext_helper());
                    fflush(stdout);
                }
            /* Match C mr_doExt: 6 (version) → 8 (appInfo) → 0 (mrc_init).
             * Redo code 6 from host in case Lua wrote it against the wrong RW. */
            typedef struct {
                int32_t id;
                int32_t ver;
                char *sidName;
                int32_t ram;
            } jjfb_appinfo_t;
            /* jjfb MRP header: APPID@192 APPVER@196 (mythroad GetMrpInfoEx). */
            int32_t app_id = 400101;
            int32_t app_ver = 12;
            if (!jjfb_read_mrp_appinfo(filename, &app_id, &app_ver)) {
                printf("[JJFB_CFG36] WARN cannot read MRP header appInfo; fallback appid=%d appver=%d\n",
                       app_id, app_ver);
            }
            jjfb_appinfo_t appinfo = {app_id, app_ver, NULL, 0};
            printf("[JJFB_CFG36] nextid=482 ncode=512 napptype=12 narg=0 narg1=1 appid=%d appver=%d\n",
                   app_id, app_ver);
            const int32_t mr_ver = 1968;
            int32_t r;
            fflush(stdout);
            r = bridge_dsm_ext_call(uc, 6, NULL, mr_ver);
            printf("[JJFB_801] host version(6) ret=%d\n", r);
            r = bridge_dsm_ext_call(uc, 8, &appinfo, (int32_t)sizeof(appinfo));
            printf("[JJFB_801] host appInfo(8) ret=%d\n", r);
            r = bridge_dsm_ext_call(uc, 0, NULL, mr_ver);
            printf("[JJFB_801] host mrc_init(0) ret=%d\n", r);
            if (r == MR_SUCCESS) {
                /* v60: family app=2 creates mrc_timer at ERW+0x8C4 (see LIFECYCLE package:
                 * 30E15E→30CBBC→305404). Required before resume's 2F5390/3054A4 or timer err:1000.
                 * Not family C0 inject. Disable: JJFB_FAMILY_APP2_AFTER_INIT=0. */
                {
                    const char *app2_env = getenv("JJFB_FAMILY_APP2_AFTER_INIT");
                    int do_app2 = !(app2_env && app2_env[0] == '0');
                    if (do_app2)
                        jjfb_lifecycle_family_app(uc, 2);
                    else
                        printf("[JJFB_V60_LIFECYCLE] family app=2 skipped "
                               "(JJFB_FAMILY_APP2_AFTER_INIT=0)\n");
                }
                /* v59: robotol code=5 = mrc_resume → 304B5A → register 2F5405.
                 * Platform lifecycle after init; not FORCE ui_mode / not C0 inject.
                 * Disable with JJFB_MRC_RESUME_AFTER_INIT=0. */
                {
                    const char *resume_env = getenv("JJFB_MRC_RESUME_AFTER_INIT");
                    int do_resume = !(resume_env && resume_env[0] == '0');
                    if (do_resume) {
                        int32_t rr = bridge_dsm_ext_call(uc, 5, NULL, 0);
                        printf("[JJFB_V59_LIFECYCLE] mrc_resume(5) after init ret=%d "
                               "(targets robotol 304B5A→2F5390 register path)\n", rr);
                    } else {
                        printf("[JJFB_V59_LIFECYCLE] mrc_resume skipped "
                               "(JJFB_MRC_RESUME_AFTER_INIT=0)\n");
                    }
                }
                /* Let SDL timer loop drive continuous code=2 (see timer()). */
                jjfb_arm_robotol_timer(50);
            }
            fflush(stdout);
            } /* robotol-loaded guard */
        }
    }
    return MR_SUCCESS;
}