#include "gwy_launcher/jjfbol_scope.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define JJFBOL_SCOPE_STACK 16
#define JJFBOL_SCOPE_PATH 512
#define JJFBOL_SCOPE_STEM 128

typedef struct {
    char guest_path[JJFBOL_SCOPE_PATH];
    char stem[JJFBOL_SCOPE_STEM];
} JjfbolScopeFrame;

static JjfbolScopeFrame g_stack[JJFBOL_SCOPE_STACK];
static int g_depth;
static uint64_t g_generation = 1;

static int is_jjfbol_mrp_guest(const char *guest_path, char *stem_out, size_t stem_cap) {
    const char *p;
    const char *slash;
    const char *name;
    size_t n;
    char norm[JJFBOL_SCOPE_PATH];
    size_t i, j;

    if (!guest_path || !guest_path[0]) return 0;
    for (i = 0, j = 0; guest_path[i] && j + 1 < sizeof(norm); i++) {
        char c = guest_path[i];
        if (c == '\\') c = '/';
        norm[j++] = (char)tolower((unsigned char)c);
    }
    norm[j] = 0;
    p = strstr(norm, "gwy/jjfbol/");
    if (!p) return 0;
    name = p + strlen("gwy/jjfbol/");
    if (!name[0]) return 0;
    n = strlen(name);
    if (n < 5) return 0;
    if (!(name[n - 4] == '.' && name[n - 3] == 'm' && name[n - 2] == 'r' && name[n - 1] == 'p'))
        return 0;
    /* reject nested path */
    for (slash = name; *slash; slash++) {
        if (*slash == '/') return 0;
    }
    if (stem_out && stem_cap) {
        size_t sn = n - 4;
        if (sn >= stem_cap) sn = stem_cap - 1;
        /* preserve original stem casing from guest_path basename */
        {
            const char *orig = guest_path;
            const char *base = guest_path;
            while (*orig) {
                if (*orig == '/' || *orig == '\\') base = orig + 1;
                orig++;
            }
            sn = strlen(base);
            if (sn >= 4) sn -= 4;
            if (sn >= stem_cap) sn = stem_cap - 1;
            memcpy(stem_out, base, sn);
            stem_out[sn] = 0;
        }
    }
    return 1;
}

void jjfbol_scope_reset(void) {
    memset(g_stack, 0, sizeof(g_stack));
    g_depth = 0;
}

void jjfbol_scope_bump_generation(void) {
    g_generation++;
    jjfbol_scope_reset();
    printf("[JJFBOL_SCOPE] generation=%llu cleared evidence=OBSERVED\n",
           (unsigned long long)g_generation);
    fflush(stdout);
}

uint64_t jjfbol_scope_generation(void) { return g_generation; }

void jjfbol_scope_on_open(const char *guest_path) {
    char stem[JJFBOL_SCOPE_STEM];
    if (!is_jjfbol_mrp_guest(guest_path, stem, sizeof(stem))) return;
    if (g_depth >= JJFBOL_SCOPE_STACK) {
        /* drop oldest */
        memmove(&g_stack[0], &g_stack[1], sizeof(g_stack) - sizeof(g_stack[0]));
        g_depth = JJFBOL_SCOPE_STACK - 1;
    }
    memset(&g_stack[g_depth], 0, sizeof(g_stack[g_depth]));
    snprintf(g_stack[g_depth].guest_path, sizeof(g_stack[g_depth].guest_path), "%s", guest_path);
    snprintf(g_stack[g_depth].stem, sizeof(g_stack[g_depth].stem), "%s", stem);
    g_depth++;
    printf("[JJFBOL_SCOPE] open stem=%s depth=%d gen=%llu evidence=OBSERVED\n", stem, g_depth,
           (unsigned long long)g_generation);
    fflush(stdout);
}

void jjfbol_scope_on_close(const char *guest_path) {
    char stem[JJFBOL_SCOPE_STEM];
    int i;
    if (!is_jjfbol_mrp_guest(guest_path, stem, sizeof(stem))) return;
    for (i = g_depth - 1; i >= 0; i--) {
        if (strcmp(g_stack[i].stem, stem) == 0 ||
            strcmp(g_stack[i].guest_path, guest_path) == 0) {
            if (i + 1 < g_depth)
                memmove(&g_stack[i], &g_stack[i + 1],
                        (size_t)(g_depth - i - 1) * sizeof(g_stack[0]));
            g_depth--;
            memset(&g_stack[g_depth], 0, sizeof(g_stack[0]));
            printf("[JJFBOL_SCOPE] close stem=%s depth=%d gen=%llu evidence=OBSERVED\n", stem,
                   g_depth, (unsigned long long)g_generation);
            fflush(stdout);
            return;
        }
    }
}

const char *jjfbol_scope_active_package(void) {
    if (g_depth <= 0) return NULL;
    return g_stack[g_depth - 1].stem;
}

const char *jjfbol_scope_active_guest_path(void) {
    if (g_depth <= 0) return NULL;
    return g_stack[g_depth - 1].guest_path;
}
