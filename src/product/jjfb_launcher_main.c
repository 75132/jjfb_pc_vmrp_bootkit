/*
 * JJFB product launcher: double-click entry that shows a status window,
 * auto-loads profiles/jjfb.json + gwy/jjfb.mrp, and spawns the Gwy runtime.
 * Trace/research modes are not exposed here.
 */
#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/launch_descriptor.h"
#include "gwy_launcher/launch_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define JJFB_CFG_INDEX 36u
#define JJFB_TIMER_ID 1
#define IDC_PATH 1001
#define IDC_STAGE 1002
#define IDC_HINT 1003

typedef enum JjfbStage {
    STAGE_INIT = 0,
    STAGE_LOADING_PROFILE,
    STAGE_LOADING_MRP,
    STAGE_LOADING_EXT,
    STAGE_STARTING_VM,
    STAGE_WAITING_FIRST_FRAME,
    STAGE_RUNNING,
    STAGE_EXITED,
    STAGE_ERROR
} JjfbStage;

typedef struct JjfbLauncherState {
    HWND hwnd;
    HWND path_label;
    HWND stage_label;
    HWND hint_label;
    HANDLE child;
    unsigned long child_pid;
    JjfbStage stage;
    int debug;
    int test_pattern;
    int closing;
    char repo_root[MAX_PATH];
    char profile_path[MAX_PATH];
    char resource_root[MAX_PATH];
    char mrp_host[MAX_PATH];
    char vmrp_exe[MAX_PATH];
    char vmrp_cwd[MAX_PATH];
    char error_msg[512];
} JjfbLauncherState;

static const char *stage_text(JjfbStage s) {
    switch (s) {
    case STAGE_INIT: return "Initializing";
    case STAGE_LOADING_PROFILE: return "Loading profile";
    case STAGE_LOADING_MRP: return "Loading MRP";
    case STAGE_LOADING_EXT: return "Loading EXT";
    case STAGE_STARTING_VM: return "Starting VM";
    case STAGE_WAITING_FIRST_FRAME: return "Waiting for first frame";
    case STAGE_RUNNING: return "Runtime running";
    case STAGE_EXITED: return "Runtime exited";
    case STAGE_ERROR: return "Error";
    default: return "Unknown";
    }
}

static int file_exists(const char *path) {
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int dir_exists(const char *path) {
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void path_join(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    if (n > 0 && (a[n - 1] == '\\' || a[n - 1] == '/'))
        snprintf(out, cap, "%s%s", a, b);
    else
        snprintf(out, cap, "%s\\%s", a, b);
}

static void path_dirname(char *path) {
    char *slash = strrchr(path, '\\');
    char *slash2 = strrchr(path, '/');
    if (slash2 && (!slash || slash2 > slash)) slash = slash2;
    if (slash) *slash = 0;
}

static int find_repo_root(char *out, size_t cap) {
    char cur[MAX_PATH];
    char probe[MAX_PATH];
    int i;

    if (!GetModuleFileNameA(NULL, cur, (DWORD)sizeof(cur))) return 0;
    path_dirname(cur);
    for (i = 0; i < 8; i++) {
        path_join(probe, sizeof(probe), cur, "profiles\\jjfb.json");
        if (file_exists(probe)) {
            snprintf(out, cap, "%s", cur);
            return 1;
        }
        path_dirname(cur);
        if (!cur[0]) break;
    }
    return 0;
}

static void set_stage(JjfbLauncherState *st, JjfbStage stage) {
    char line[160];
    st->stage = stage;
    snprintf(line, sizeof(line), "Stage: %s", stage_text(stage));
    if (st->stage_label) SetWindowTextA(st->stage_label, line);
    printf("[JJFB_LAUNCHER] stage=%s\n", stage_text(stage));
    fflush(stdout);
}

static void set_path_label(JjfbLauncherState *st) {
    char line[MAX_PATH + 64];
    if (!st->path_label) return;
    if (st->mrp_host[0])
        snprintf(line, sizeof(line), "MRP: %s", st->mrp_host);
    else
        snprintf(line, sizeof(line), "MRP: (resolving)");
    SetWindowTextA(st->path_label, line);
}

static void stop_child(JjfbLauncherState *st) {
    if (!st->child) return;
    if (WaitForSingleObject(st->child, 0) == WAIT_TIMEOUT) {
        TerminateProcess(st->child, 0);
        WaitForSingleObject(st->child, 3000);
    }
    CloseHandle(st->child);
    st->child = NULL;
    st->child_pid = 0;
}

static void apply_product_env(JjfbLauncherState *st) {
    SetEnvironmentVariableA("GWY_PROFILE", st->profile_path);
    SetEnvironmentVariableA("GWY_WINDOW_TITLE", "JJFB Launcher");
    SetEnvironmentVariableA("JJFB_LAUNCH_SOURCE", "jjfb_launcher");
    SetEnvironmentVariableA("JJFB_PRIMARY_TARGET", "gwy/jjfb.mrp");
    SetEnvironmentVariableA("JJFB_PRODUCT_DESCRIPTOR_DIRECT", "1");
    SetEnvironmentVariableA("JJFB_PACKAGE_SCOPED_CLOAD", "1");
    SetEnvironmentVariableA("JJFB_MEMBER_VIEW_PRIMARY", "game_package");
    SetEnvironmentVariableA("JJFB_EXTCHUNK_PROVIDER", "game_package");
    SetEnvironmentVariableA("JJFB_ER_RW_BIND_RESTORE", "game_package");
    SetEnvironmentVariableA("GWY_MODULE_R9_SWITCH", "1");
    SetEnvironmentVariableA("GWY_CALLBACK_FRAME", "1");
    SetEnvironmentVariableA("JJFB_E5_SCHEDULER_MODE", "1");
    SetEnvironmentVariableA("JJFB_GAME_SELF_PATCH", "0");
    if (st->test_pattern)
        SetEnvironmentVariableA("GWY_HOST_TEST_PATTERN", "1");
    else
        SetEnvironmentVariableA("GWY_HOST_TEST_PATTERN", NULL);
}

static int resolve_paths(JjfbLauncherState *st) {
    CompatibilityProfile profile;
    LauncherError err;

    set_stage(st, STAGE_LOADING_PROFILE);
    if (!find_repo_root(st->repo_root, sizeof(st->repo_root))) {
        snprintf(st->error_msg, sizeof(st->error_msg),
                 "Cannot find repo root (profiles/jjfb.json).");
        return 0;
    }
    path_join(st->profile_path, sizeof(st->profile_path), st->repo_root, "profiles\\jjfb.json");
    path_join(st->resource_root, sizeof(st->resource_root), st->repo_root,
              "game_files\\mythroad\\320x480");
    path_join(st->mrp_host, sizeof(st->mrp_host), st->resource_root, "gwy\\jjfb.mrp");
    path_join(st->vmrp_cwd, sizeof(st->vmrp_cwd), st->repo_root, "out\\vmrp_run");
    path_join(st->vmrp_exe, sizeof(st->vmrp_exe), st->vmrp_cwd, "main.exe");

    if (!file_exists(st->profile_path)) {
        snprintf(st->error_msg, sizeof(st->error_msg), "Missing profile: %s", st->profile_path);
        return 0;
    }
    if (compatibility_profile_load_json_file(st->profile_path, &profile, &err) != L_OK) {
        snprintf(st->error_msg, sizeof(st->error_msg), "Profile load failed: %s", err.message);
        return 0;
    }
    if (!dir_exists(st->resource_root)) {
        snprintf(st->error_msg, sizeof(st->error_msg), "Missing resource root: %s", st->resource_root);
        return 0;
    }
    if (!file_exists(st->mrp_host)) {
        snprintf(st->error_msg, sizeof(st->error_msg), "Missing MRP: %s", st->mrp_host);
        return 0;
    }
    if (!file_exists(st->vmrp_exe)) {
        snprintf(st->error_msg, sizeof(st->error_msg),
                 "Missing runtime: %s\nBuild with RUN_BUILD_VMRP.ps1 -Mode Gwy", st->vmrp_exe);
        return 0;
    }
    set_path_label(st);
    return 1;
}

static int start_runtime(JjfbLauncherState *st) {
    LaunchDescriptor desc;
    LaunchExpectations ex;
    LauncherError err;
    LauncherStatus stv;
    char manifest[MAX_PATH];
    void *proc = NULL;
    unsigned long pid = 0;

    set_stage(st, STAGE_LOADING_MRP);
    memset(&ex, 0, sizeof(ex));
    ex.has_target = 1;
    snprintf(ex.target_mrp, sizeof(ex.target_mrp), "%s", "gwy/jjfb.mrp");

    stv = launch_descriptor_build(st->resource_root, JJFB_CFG_INDEX, "jjfb", &ex, &desc, &err);
    if (stv != L_OK) {
        snprintf(st->error_msg, sizeof(st->error_msg), "%s\n%s", err.message, err.detail);
        return 0;
    }

    set_stage(st, STAGE_LOADING_EXT);
    apply_product_env(st);
    path_join(manifest, sizeof(manifest), st->vmrp_cwd, "launch_manifest.json");

    set_stage(st, STAGE_STARTING_VM);
    stv = gwy_launch_spawn_vmrp_ex(&desc, st->vmrp_exe, st->vmrp_cwd, manifest, &proc, &pid, &err);
    if (stv != L_OK) {
        snprintf(st->error_msg, sizeof(st->error_msg), "%s\n%s", err.message, err.detail);
        return 0;
    }
    st->child = (HANDLE)proc;
    st->child_pid = pid;
    set_stage(st, STAGE_WAITING_FIRST_FRAME);
    if (st->hint_label) {
        SetWindowTextA(st->hint_label,
                       "Game window should appear. Close this window to stop the runtime.");
    }
    return 1;
}

static void poll_child(JjfbLauncherState *st) {
    DWORD code = 0;
    if (!st->child || st->closing) return;
    if (WaitForSingleObject(st->child, 0) != WAIT_OBJECT_0) {
        /* After a few seconds without exit, treat as steady running. */
        if (st->stage == STAGE_WAITING_FIRST_FRAME) {
            static int ticks;
            ticks++;
            if (ticks > 40) { /* ~2s at 50ms */
                set_stage(st, STAGE_RUNNING);
            }
        }
        return;
    }
    GetExitCodeProcess(st->child, &code);
    CloseHandle(st->child);
    st->child = NULL;
    st->child_pid = 0;
    set_stage(st, STAGE_EXITED);
    if (st->hint_label) {
        char tip[128];
        snprintf(tip, sizeof(tip), "Runtime exited (code=%lu). Close this window.",
                 (unsigned long)code);
        SetWindowTextA(st->hint_label, tip);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    JjfbLauncherState *st = (JjfbLauncherState *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
        st = (JjfbLauncherState *)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        st->hwnd = hwnd;
        st->path_label = CreateWindowA("STATIC", "MRP: (resolving)",
                                       WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       16, 16, 520, 40, hwnd, (HMENU)IDC_PATH, cs->hInstance, NULL);
        st->stage_label = CreateWindowA("STATIC", "Stage: Initializing",
                                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                                        16, 64, 520, 24, hwnd, (HMENU)IDC_STAGE, cs->hInstance, NULL);
        st->hint_label = CreateWindowA("STATIC", "Preparing JJFB launch…",
                                       WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       16, 100, 520, 48, hwnd, (HMENU)IDC_HINT, cs->hInstance, NULL);
        SetTimer(hwnd, JJFB_TIMER_ID, 50, NULL);
        PostMessageA(hwnd, WM_USER + 1, 0, 0); /* kick async start */
        return 0;
    }
    case WM_USER + 1:
        if (!resolve_paths(st)) {
            set_stage(st, STAGE_ERROR);
            if (st->hint_label) SetWindowTextA(st->hint_label, st->error_msg);
            MessageBoxA(hwnd, st->error_msg, "JJFB Launcher", MB_ICONERROR);
            return 0;
        }
        if (!start_runtime(st)) {
            set_stage(st, STAGE_ERROR);
            if (st->hint_label) SetWindowTextA(st->hint_label, st->error_msg);
            MessageBoxA(hwnd, st->error_msg, "JJFB Launcher", MB_ICONERROR);
            return 0;
        }
        return 0;
    case WM_TIMER:
        if (wParam == JJFB_TIMER_ID) poll_child(st);
        return 0;
    case WM_CLOSE:
        st->closing = 1;
        KillTimer(hwnd, JJFB_TIMER_ID);
        stop_child(st);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st) {
            st->closing = 1;
            KillTimer(hwnd, JJFB_TIMER_ID);
            stop_child(st);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int parse_args(JjfbLauncherState *st, int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
            st->debug = 1;
        else if (strcmp(argv[i], "--test-pattern") == 0)
            st->test_pattern = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("JJFB_Launcher [--debug] [--test-pattern]");
            puts("  Default: load profiles/jjfb.json and start gwy/jjfb.mrp");
            puts("  --debug         attach console log");
            puts("  --test-pattern  host framebuffer test pattern (not JJFB first frame)");
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    JjfbLauncherState st;
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    HINSTANCE hi = GetModuleHandleA(NULL);

    memset(&st, 0, sizeof(st));
    if (parse_args(&st, argc, argv)) return 0;

    if (st.debug) {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = "JjfbLauncherStatus";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "JjfbLauncherStatus", "JJFB Launcher",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 560, 200,
                           NULL, NULL, hi, &st);
    if (!hwnd) {
        MessageBoxA(NULL, "CreateWindow failed", "JJFB Launcher", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmd, int show) {
    (void)hi;
    (void)prev;
    (void)cmd;
    (void)show;
    return main(__argc, __argv);
}

#else
int main(void) {
    fputs("JJFB_Launcher is Windows-only.\n", stderr);
    return 1;
}
#endif
