#include "gwy_launcher/game_catalog.h"
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

typedef struct PickerState {
    const GwyGameCatalog *catalog;
    const char *vmrp_exe;
    const char *vmrp_cwd;
    HWND list;
    int selected;
} PickerState;

static wchar_t *utf8_to_wide(const char *utf8) {
    int n;
    wchar_t *w;
    if (!utf8) utf8 = "";
    n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0) {
        w = (wchar_t *)malloc(sizeof(wchar_t));
        if (w) w[0] = 0;
        return w;
    }
    w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

static void launch_selected(PickerState *st) {
    const GwyGameEntry *e;
    LaunchDescriptor desc;
    LaunchExpectations ex;
    LauncherError err;
    LauncherStatus stv;
    char manifest[1024];

    if (!st || st->selected < 0 || (size_t)st->selected >= st->catalog->count) return;
    e = &st->catalog->entries[st->selected];

    memset(&ex, 0, sizeof(ex));
    ex.has_target = 1;
    snprintf(ex.target_mrp, sizeof(ex.target_mrp), "%s", e->target_mrp);

    stv = launch_descriptor_build(st->catalog->resource_root, e->cfg_index,
                                  "catalog", &ex, &desc, &err);
    if (stv != L_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s\n%s", err.message, err.detail);
        MessageBoxA(NULL, msg, "launch_descriptor failed", MB_ICONERROR);
        return;
    }

    snprintf(manifest, sizeof(manifest), "%s\\launch_manifest.json", st->vmrp_cwd);
    stv = gwy_launch_spawn_vmrp(&desc, st->vmrp_exe, st->vmrp_cwd, manifest, &err);
    if (stv != L_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s\n%s", err.message, err.detail);
        MessageBoxA(NULL, msg, "launch failed", MB_ICONERROR);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PickerState *st = (PickerState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        size_t i;
        st = (PickerState *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        st->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
                                   10, 10, 460, 520, hwnd, (HMENU)1001, cs->hInstance, NULL);
        for (i = 0; i < st->catalog->count; i++) {
            const GwyGameEntry *e = &st->catalog->entries[i];
            char line[320];
            wchar_t *wline;
            const char *title = e->title_utf8[0] ? e->title_utf8 :
                                 (e->display_name_utf8[0] ? e->display_name_utf8 : e->target_mrp);
            snprintf(line, sizeof(line), "[%u] %s  (%s)", e->cfg_index, title, e->target_mrp);
            wline = utf8_to_wide(line);
            if (wline) {
                SendMessageW(st->list, LB_ADDSTRING, 0, (LPARAM)wline);
                free(wline);
            }
        }
        if (st->catalog->count > 0) {
            SendMessageW(st->list, LB_SETCURSEL, 0, 0);
            st->selected = 0;
        }
        CreateWindowW(L"BUTTON", L"启动 (vmrp)", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      10, 540, 140, 32, hwnd, (HMENU)1002, cs->hInstance, NULL);
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE,
                      160, 540, 100, 32, hwnd, (HMENU)1003, cs->hInstance, NULL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == LBN_DBLCLK) {
            st->selected = (int)SendMessageW(st->list, LB_GETCURSEL, 0, 0);
            launch_selected(st);
            return 0;
        }
        if (LOWORD(wParam) == 1002) {
            st->selected = (int)SendMessageW(st->list, LB_GETCURSEL, 0, 0);
            launch_selected(st);
            return 0;
        }
        if (LOWORD(wParam) == 1003) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int gwy_games_picker_run(const GwyGameCatalog *catalog,
                         const char *vmrp_exe,
                         const char *vmrp_cwd) {
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    PickerState st;
    HINSTANCE hi = GetModuleHandleW(NULL);

    memset(&st, 0, sizeof(st));
    st.catalog = catalog;
    st.vmrp_exe = vmrp_exe;
    st.vmrp_cwd = vmrp_cwd;
    st.selected = -1;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"GwyGamesPicker";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(0, L"GwyGamesPicker", L"冒泡网游列表 (本地 cfg)",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 500, 620,
                           NULL, NULL, hi, &st);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

#else
int gwy_games_picker_run(const GwyGameCatalog *catalog,
                         const char *vmrp_exe,
                         const char *vmrp_cwd) {
    (void)vmrp_exe;
    (void)vmrp_cwd;
    size_t i;
    for (i = 0; i < catalog->count; i++) {
        const GwyGameEntry *e = &catalog->entries[i];
        printf("[%u] %s (%s)\n", e->cfg_index,
               e->title_utf8[0] ? e->title_utf8 : e->target_mrp,
               e->target_mrp);
    }
    return 0;
}
#endif
