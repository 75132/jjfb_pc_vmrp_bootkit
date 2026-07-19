#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int seed_extracted(ModuleRegistry *reg,
                          const char *requested,
                          const char *resolved,
                          ExtResolveStrategy strategy,
                          uint32_t size,
                          uint64_t *out_id) {
    ExtResolvedMember res;
    MrpMember member;
    ByteBuffer buf;
    LauncherError err;
    const GwyLoadedModule *m;
    uint8_t *blob;

    memset(&member, 0, sizeof(member));
    snprintf(member.name, sizeof(member.name), "%s", resolved);
    member.stored_size = size;
    member.unpacked_size = size;
    member.unpacked_known = 1;
    member.compression = MRP_COMP_RAW;

    memset(&res, 0, sizeof(res));
    snprintf(res.requested_name, sizeof(res.requested_name), "%s", requested);
    snprintf(res.resolved_name, sizeof(res.resolved_name), "%s", resolved);
    res.strategy = strategy;
    res.member = &member;
    snprintf(res.package_guest_path, sizeof(res.package_guest_path), "%s", "gwy/jjfb.mrp");

    blob = (uint8_t *)malloc(size ? size : 1);
    if (!blob) return 1;
    memset(blob, 0xA5, size);
    byte_buffer_init(&buf);
    if (byte_buffer_append(&buf, blob, size) == 0) {
        free(blob);
        byte_buffer_free(&buf);
        return 1;
    }
    free(blob);
    if (module_registry_register_resolved(reg, &res, &buf, &err) != L_OK) {
        byte_buffer_free(&buf);
        return 1;
    }
    byte_buffer_free(&buf);
    m = module_registry_find(reg, requested);
    if (!m || m->state != GWY_MODULE_EXTRACTED) return 1;
    *out_id = m->module_id;
    return 0;
}

int main(void) {
    ExtLoader *loader;
    ModuleRegistry reg;
    ExtLoadSession *session;
    LauncherError err;
    uint64_t mid_robotol = 0;
    uint64_t mid_loader = 0;
    const GwyLoadedModule *m;
    GwyModuleMapping map;
    const GwyLoadedModule *dsm;

    loader = gwy_ext_loader_ensure();
    ext_loader_destroy_all(loader);
    module_registry_init(&reg, "gwy/jjfb.mrp",
                         "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036");
    module_registry_set_provenance(&reg, "gwy/jjfb.mrp", "generated_member_view",
                                   "scoped_member_alias");
    ext_loader_bind_registry(loader, &reg);

    if (seed_extracted(&reg, "cfunction.ext", "robotol.ext", EXT_RESOLVE_PROFILE_ALIAS, 16,
                       &mid_robotol) != 0) {
        return 1;
    }
    if (ext_loader_seed_from_registry(loader, &err) != L_OK) return 1;
    session = ext_loader_find_session_by_module(loader, mid_robotol);
    if (!session || ext_loader_session_context(session)->load_id == 0) return 1;

    if (ext_loader_register(session, 0xABC0, 0xABC0, &err) == L_OK) return 1;

    memset(&map, 0, sizeof(map));
    map.guest_code_base = 0xABC0;
    map.guest_code_size = 16;
    if (ext_loader_map(session, &map, &err) != L_OK) return 1;
    if (ext_loader_register(session, 0xABC0, 0xABC8, &err) != L_OK) return 1;
    if (ext_loader_call_entry(session, 0, 0, 0, 1, &err) != L_OK) return 1;
    m = module_registry_find_by_id(&reg, mid_robotol);
    if (!m || m->state != GWY_MODULE_ENTRY_CALLED) return 1;
    if (m->origin != MODULE_ORIGIN_MRP_MEMBER) return 1;

    if (seed_extracted(&reg, "mrc_loader.ext", "mrc_loader.ext", EXT_RESOLVE_EXACT, 32,
                       &mid_loader) != 0) {
        return 1;
    }
    if (ext_loader_begin(loader, mid_loader, NULL, 0, &session, &err) != L_OK) return 1;
    ext_loader_on_alloc(loader, 0x5000, 32);
    ext_loader_on_c_function_new(loader, 0x5000, 64, 0x6000, 0x7000, 128, 0);
    m = module_registry_find_by_id(&reg, mid_loader);
    if (!m || m->state < GWY_MODULE_REGISTERED) return 1;
    if (m->map.helper_address != 0x5000) return 1;
    if (module_registry_find_by_id(&reg, mid_robotol)->state != GWY_MODULE_ENTRY_CALLED) return 1;

    ext_loader_on_c_function_new(loader, 0x80008, 100, 0x80100, 0x90000, 256, 0);
    dsm = module_registry_find(&reg, "dsm:cfunction.ext");
    if (!dsm || dsm->origin != MODULE_ORIGIN_DSM) return 1;
    if (dsm->state < GWY_MODULE_REGISTERED) return 1;
    if (dsm->map.helper_address != 0x80008) return 1;
    m = module_registry_find(&reg, "cfunction.ext");
    if (!m || m->origin != MODULE_ORIGIN_MRP_MEMBER) return 1;
    if (strcmp(m->resolved_name, "robotol.ext") != 0) return 1;

    /* DOCUMENTED: cacheSync align-down vs --- ext: @ raw MRPG base. */
    {
        uint64_t mid_gl = 0;
        const uint32_t extracted = 91532u;
        const uint32_t sync_size = 91616u; /* (extracted + 0x1F*3) & ~0x1F */
        const uint32_t aligned = 0x2D4340u;
        const uint32_t raw = 0x2D4354u;
        if (seed_extracted(&reg, "gamelist.ext", "gamelist.ext", EXT_RESOLVE_EXACT, extracted,
                           &mid_gl) != 0)
            return 1;
        if (ext_loader_begin(loader, mid_gl, NULL, 0, &session, &err) != L_OK) return 1;
        ext_loader_on_code_image(loader, aligned, sync_size);
        m = module_registry_find_by_id(&reg, mid_gl);
        if (!m || m->map.guest_code_base != aligned) return 1;
        if (m->entries.header_entry_candidate != aligned + 8u) return 1;
        ext_loader_on_ext_image_raw(loader, raw);
        m = module_registry_find_by_id(&reg, mid_gl);
        if (!m || m->map.guest_code_base != raw) return 1;
        if (m->map.guest_code_size != sync_size - (raw - aligned)) return 1;
        if (m->entries.header_entry_candidate != raw + 8u) return 1;
        if (m->entries.image_base != raw) return 1;
    }

    ext_loader_destroy_all(loader);
    puts("test_ext_loader OK");
    return 0;
}
