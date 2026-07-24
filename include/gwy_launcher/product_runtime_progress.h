#ifndef GWY_LAUNCHER_PRODUCT_RUNTIME_PROGRESS_H
#define GWY_LAUNCHER_PRODUCT_RUNTIME_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Observe-only runtime milestone IPC for JJFB_Launcher status window.
 * Writes JSONL lines to GWY_RUNTIME_PROGRESS_PATH (default:
 * out/vmrp_run/runtime_progress.jsonl). Never mutates guest state.
 */

void product_runtime_progress_reset(void);
void product_runtime_progress_set_run_id(const char *run_id);
const char *product_runtime_progress_run_id(void);

/* Append one milestone. source/details optional. */
void product_runtime_progress_emit(const char *milestone,
                                   const char *source,
                                   const char *details);

#ifdef __cplusplus
}
#endif

#endif
