#include "gwy_launcher/mrp_runtime_stack.h"
#include <stdio.h>
#include <string.h>

static MrpRuntimeStack g_stack;

void mrp_runtime_stack_reset(MrpRuntimeStack *st) {
    if (!st) return;
    memset(st, 0, sizeof(*st));
}

MrpRuntimeStack *mrp_runtime_stack_global(void) { return &g_stack; }

MrpRuntimeFrameRole mrp_runtime_stack_role_for_package(const char *package) {
    if (!package || !package[0]) return MRP_FRAME_NONE;
    if (strstr(package, "jjfb")) return MRP_FRAME_CHILD_JJFB;
    if (strstr(package, "gbrwcore")) return MRP_FRAME_PARENT_GBRWCORE;
    if (strstr(package, "gbrwshell")) return MRP_FRAME_PARENT_GBRWSHELL;
    if (strstr(package, "gamelist")) return MRP_FRAME_PARENT_GAMELIST;
    if (strstr(package, "gwy.mrp") || (strstr(package, "gwy") && !strstr(package, "/")))
        return MRP_FRAME_PARENT_GWY;
    return MRP_FRAME_OTHER;
}

static const char *role_name(MrpRuntimeFrameRole r) {
    switch (r) {
    case MRP_FRAME_PARENT_GWY: return "parent_gwy";
    case MRP_FRAME_PARENT_GBRWCORE: return "parent_gbrwcore";
    case MRP_FRAME_PARENT_GBRWSHELL: return "parent_gbrwshell";
    case MRP_FRAME_PARENT_GAMELIST: return "parent_gamelist";
    case MRP_FRAME_CHILD_JJFB: return "child_jjfb";
    case MRP_FRAME_OTHER: return "other";
    default: return "none";
    }
}

int mrp_runtime_stack_push(MrpRuntimeStack *st, const char *package, const char *primary_ext,
                           uint32_t r9, uint32_t erw, uint32_t module_generation) {
    MrpRuntimeFrame *f;
    uint32_t parent_id = 0;
    if (!st || st->depth >= MRP_RUNTIME_STACK_MAX) return 0;
    if (st->depth > 0) parent_id = st->frames[st->depth - 1].frame_id;
    f = &st->frames[st->depth++];
    memset(f, 0, sizeof(*f));
    f->frame_id = (uint32_t)st->depth;
    f->parent_frame_id = parent_id;
    f->role = mrp_runtime_stack_role_for_package(package);
    snprintf(f->package, sizeof(f->package), "%s", package ? package : "");
    snprintf(f->primary_ext, sizeof(f->primary_ext), "%s", primary_ext ? primary_ext : "");
    f->r9 = r9;
    f->erw = erw;
    f->module_generation = module_generation;
    f->active = 1;
    printf("[MRP_RUNTIME_STACK] push depth=%d frame_id=%u parent_frame_id=%u role=%s "
           "package=%s ext=%s r9=0x%X erw=0x%X gen=%u evidence=OBSERVED\n",
           st->depth, f->frame_id, f->parent_frame_id, role_name(f->role), f->package,
           f->primary_ext, f->r9, f->erw, f->module_generation);
    fflush(stdout);
    return 1;
}

int mrp_runtime_stack_bind_top(MrpRuntimeStack *st, uint64_t module_id, uint32_t helper_pc,
                               uint32_t p_guest, uint32_t chunk_guest) {
    MrpRuntimeFrame *f = mrp_runtime_stack_top(st);
    if (!f) return 0;
    if (module_id) f->module_id = module_id;
    if (helper_pc) f->helper_pc = helper_pc;
    if (p_guest) f->p_guest = p_guest;
    if (chunk_guest) f->chunk_guest = chunk_guest;
    printf("[MRP_RUNTIME_STACK] bind frame_id=%u module_id=%llu helper=0x%X p=0x%X chunk=0x%X "
           "evidence=OBSERVED\n",
           f->frame_id, (unsigned long long)f->module_id, f->helper_pc, f->p_guest,
           f->chunk_guest);
    fflush(stdout);
    return 1;
}

int mrp_runtime_stack_update_top(MrpRuntimeStack *st, uint32_t r9, uint32_t erw,
                                 uint32_t callback_registry, uint32_t transport_provider,
                                 uint32_t provider_101ab, uint32_t timer_owner) {
    MrpRuntimeFrame *f = mrp_runtime_stack_top(st);
    if (!f) return 0;
    if (r9) f->r9 = r9;
    if (erw) f->erw = erw;
    if (callback_registry) f->callback_registry = callback_registry;
    if (transport_provider) f->transport_provider = transport_provider;
    if (provider_101ab) f->provider_101ab = provider_101ab;
    if (timer_owner) f->timer_owner = timer_owner;
    return 1;
}

MrpRuntimeFrame *mrp_runtime_stack_top(MrpRuntimeStack *st) {
    if (!st || st->depth <= 0) return NULL;
    return &st->frames[st->depth - 1];
}

MrpRuntimeFrame *mrp_runtime_stack_parent(MrpRuntimeStack *st) {
    if (!st || st->depth < 2) return NULL;
    return &st->frames[st->depth - 2];
}

int mrp_runtime_stack_note_nested_jjfb(MrpRuntimeStack *st, const char *target,
                                       const char *param) {
    if (!st) return 0;
    st->nested_jjfb_intercepted = 1;
    snprintf(st->last_nested_target, sizeof(st->last_nested_target), "%s",
             target ? target : "gwy/jjfb.mrp");
    snprintf(st->last_nested_param, sizeof(st->last_nested_param), "%s", param ? param : "");
    printf("[MRP_RUNTIME_STACK] nested_jjfb target=%s param=\"%.120s\" keep_parent=yes "
           "evidence=OBSERVED\n",
           st->last_nested_target, st->last_nested_param);
    fflush(stdout);
    return 1;
}

int mrp_runtime_stack_write_json(const MrpRuntimeStack *st, const char *path) {
    FILE *fp;
    int i;
    if (!st || !path || !path[0]) return -1;
    fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"depth\": %d,\n", st->depth);
    fprintf(fp, "  \"nested_jjfb_intercepted\": %s,\n",
            st->nested_jjfb_intercepted ? "true" : "false");
    fprintf(fp, "  \"last_nested_target\": \"%s\",\n", st->last_nested_target);
    fprintf(fp, "  \"last_nested_param\": \"%s\",\n", st->last_nested_param);
    fprintf(fp, "  \"frames\": [\n");
    for (i = 0; i < st->depth; i++) {
        const MrpRuntimeFrame *f = &st->frames[i];
        fprintf(fp,
                "    {\"index\":%d,\"role\":\"%s\",\"package\":\"%s\",\"primary_ext\":\"%s\","
                "\"r9\":\"0x%X\",\"erw\":\"0x%X\",\"module_generation\":%u,"
                "\"callback_registry\":\"0x%X\",\"transport_provider\":\"0x%X\","
                "\"provider_101ab\":\"0x%X\",\"timer_owner\":\"0x%X\",\"active\":%s}%s\n",
                i, role_name(f->role), f->package, f->primary_ext, f->r9, f->erw,
                f->module_generation, f->callback_registry, f->transport_provider,
                f->provider_101ab, f->timer_owner, f->active ? "true" : "false",
                (i + 1 < st->depth) ? "," : "");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"note\": \"Parent frame must survive child jjfb start; do not destroy "
                "bootstrap VM.\"\n");
    fprintf(fp, "}\n");
    fclose(fp);
    return 0;
}
