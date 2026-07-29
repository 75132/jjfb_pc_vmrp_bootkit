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
#include "./header/gwy_vm_file_abi.h"
#include "./header/gwy_ext_obs_abi.h"
#include "gwy_launcher/product_runtime_progress.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static uint8_t *mrpMem;  // 模拟器的全部内存
static uint8_t *g_lowGuestMem; /* [0, CODE_ADDRESS) zero page — plat-code-as-ptr safety */
static uc_engine *uc = NULL;

/*
 * P12 evidence (JJFB family case 5 @ 0x30E186 → 0x2D9600):
 * guest passes event_code (e.g. 0x1E201) where an object pointer is loaded
 * via LDR [r0,#4] → UC_MEM_READ_UNMAPPED @ 0x1E205. Mapping [0,CODE_ADDRESS)
 * as zeroed RW matches upstream vmrp's own noted workaround and lets the
 * free-helper see a NULL secondary pointer then fall through to 0x305E08/10133.
 * Env: JJFB_MAP_LOW_GUEST_MEM=0 disables for A/B; default ON.
 */
static int map_low_guest_mem_enabled(void) {
    const char *e = getenv("JJFB_MAP_LOW_GUEST_MEM");
    if (e && e[0] == '0' && e[1] == '\0') return 0;
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
    (void)user_data;
    printf(">>> Tracing mem_invalid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
           memTypeStr(type), address, size, value);
    dumpREG(uc);
    gwy_ext_obs_mem_fault(uc, (uint32_t)type, address, (uint32_t)size, value);
    return false;
}

int freeVmrp(uc_engine *uc) {
    free(mrpMem);
    mrpMem = NULL;
    free(g_lowGuestMem);
    g_lowGuestMem = NULL;
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

    if (map_low_guest_mem_enabled()) {
        /* RW only — do not make low plat-code band executable. */
        g_lowGuestMem = (uint8_t *)calloc(1, CODE_ADDRESS);
        if (g_lowGuestMem) {
            err = uc_mem_map_ptr(uc, 0, CODE_ADDRESS, UC_PROT_READ | UC_PROT_WRITE, g_lowGuestMem);
            if (err) {
                printf("[PLATFORM_LOW_MEM_MAP] op=FAIL err=%u size=0x%X evidence=OBSERVED\n", err,
                       (unsigned)CODE_ADDRESS);
                fflush(stdout);
                free(g_lowGuestMem);
                g_lowGuestMem = NULL;
            } else {
                printf("[PLATFORM_LOW_MEM_MAP] op=MAP base=0 size=0x%X prot=RW "
                       "note=plat_code_as_ptr_safety evidence=OBSERVED\n",
                       (unsigned)CODE_ADDRESS);
                fflush(stdout);
            }
        }
    } else {
        printf("[PLATFORM_LOW_MEM_MAP] op=SKIP env=JJFB_MAP_LOW_GUEST_MEM=0 evidence=OBSERVED\n");
        fflush(stdout);
    }

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
    gwy_ext_obs_code_image(CODE_ADDRESS, (uint32_t)len);
    return MR_SUCCESS;
}

int startVmrp() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    {
        const char *rid = getenv("GWY_PRODUCT_RUN_ID");
        if (rid && rid[0]) product_runtime_progress_set_run_id(rid);
        product_runtime_progress_emit("runtime_spawned", "vmrp", "startVmrp");
    }

    uc = initVmrp();
    if (uc == NULL) {
        printf("initVmrp() fail.\n");
        product_runtime_progress_emit("runtime_error", "vmrp", "initVmrp_fail");
        return MR_FAILED;
    }
    gwy_ext_obs_bind_uc(uc);

    if (loadCode() == MR_FAILED) {
        printf("loadCode fail.\n");
        return MR_FAILED;
    }
    bridge_ext_init(uc);

    if (bridge_dsm_init(uc) == MR_SUCCESS) {
        printf("bridge_dsm_init success\n");
        dumpREG(uc);

        /* Clean GWY launcher entry: direct target MRP + cfg36-style param.
         * Default stays upstream shell (dsm_gm.mrp) unless GWY_LAUNCH=1. */
        typedef enum {
            VMRP_MODE_UPSTREAM_COMPAT = 0,
            VMRP_MODE_GWY_LAUNCHER = 1
        } VmrpRunMode;

        const char *gwy_launch = getenv("GWY_LAUNCH");
        char *filename = "dsm_gm.mrp";
        char *extName = "start.mr";
        char *startParam =
            "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
        VmrpRunMode run_mode = VMRP_MODE_UPSTREAM_COMPAT;

        static char shell_jjfb_target[160];
        static char shell_jjfb_param[320];

        if (gwy_launch && (gwy_launch[0] == '1' || gwy_launch[0] == 'y' || gwy_launch[0] == 'Y')) {
            const char *target = getenv("GWY_LAUNCH_TARGET");
            const char *param = getenv("GWY_LAUNCH_PARAM");
            run_mode = VMRP_MODE_GWY_LAUNCHER;
            filename = (target && target[0]) ? (char *)target : "gwy/jjfb.mrp";
            startParam = (param && param[0])
                             ? (char *)param
                             : "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
            /* Bind GuestVFS after firmware load, before guest MRP file I/O. Fail-fast if unbound. */
            if (gwy_vmrp_prepare_guest_vfs() != 0 || !gwy_vm_file_is_bound()) {
                printf("[JJFB_BLOCKER] GWY_LAUNCH fail-fast: GuestVFS not bound (prepare/bind failed)\n");
                fprintf(stderr, "[JJFB_BLOCKER] GWY_LAUNCH fail-fast: GuestVFS not bound (prepare/bind failed)\n");
                fflush(stdout);
                fflush(stderr);
                return MR_FAILED;
            }
            /*
             * Phase 6G: host-side shell warmup + jjfb DSM (prepare returns 1).
             * Phase 6H (gwy_guest_native_runapp): prepare returns 0; keep
             * GWY_LAUNCH_TARGET (gbrwcore) so guest shell code actually runs.
             */
            if (gwy_shell_shim_prepare_jjfb_launch(shell_jjfb_target, sizeof(shell_jjfb_target),
                                                   shell_jjfb_param, sizeof(shell_jjfb_param))) {
                filename = shell_jjfb_target;
                startParam = shell_jjfb_param;
                /* Emit summary before jjfb DSM (may block in guest event loop). */
                gwy_shell_shim_finalize("shell_context_ready_before_jjfb_dsm");
            } else {
                const char *lp = getenv("JJFB_LAUNCH_PATH");
                if (lp && (strcmp(lp, "gwy_guest_native_runapp") == 0 ||
                           strcmp(lp, "gwy_shell_core_continue") == 0)) {
                    printf("[JJFB_GWY_LAUNCH] mode=%s dsm_target=%s note=guest_shell_exec\n", lp,
                           filename);
                    fflush(stdout);
                    fflush(stderr);
                }
            }
            printf("[GWY_LAUNCH] mode=GWY_LAUNCHER bound=1\n");
            fprintf(stderr, "[GWY_LAUNCH] mode=GWY_LAUNCHER bound=1\n");
            printf("[GWY_LAUNCH] target=%s\n", filename);
            printf("[GWY_LAUNCH] param=%s\n", startParam);
            fflush(stdout);
            fflush(stderr);
        } else {
            (void)run_mode;
        }

        uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);
        printf("bridge_dsm_mr_start_dsm('%s','%s','%s'): 0x%X\n", filename, extName, startParam, ret);
        {
            char det[192];
            snprintf(det, sizeof(det), "target=%s ret=0x%X", filename, ret);
            product_runtime_progress_emit("guest_entry_called", "vmrp", det);
            product_runtime_progress_emit("waiting_for_first_frame", "vmrp", "post_start_dsm");
        }
    }
    return MR_SUCCESS;
}