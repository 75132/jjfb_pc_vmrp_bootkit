cmake_minimum_required(VERSION 3.20)
project(gwy_launcher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GenerateBuildInfo)

find_package(ZLIB REQUIRED)

set(GWY_UNICORN_ROOT "${CMAKE_SOURCE_DIR}/third_party/vmrp_upstream/windows/unicorn-1.0.2-win32")
if(NOT EXISTS "${GWY_UNICORN_ROOT}/include/unicorn/unicorn.h")
  set(GWY_UNICORN_ROOT "${CMAKE_SOURCE_DIR}/third_party/deps/unicorn-1.0.2-win32")
endif()
if(NOT EXISTS "${GWY_UNICORN_ROOT}/include/unicorn/unicorn.h")
  message(FATAL_ERROR "Unicorn not found under third_party (expected unicorn-1.0.2-win32)")
endif()

# Product core: formats / launcher / vfs / profiles / generic runtime / platform.
# Research probes (E10A / shell / SMSCFG / jjfb visual) live in research_gwy_shell.
set(GWY_LAUNCHER_CORE_SOURCES
    src/launcher/error.c
    src/launcher/byte_buffer.c
    src/launcher/launch_descriptor.c
    src/launcher/launch_runtime.c
    src/launcher/utf.c
    src/launcher/game_catalog.c
    src/vfs/guest_vfs.c
    src/formats/sha256.c
    src/formats/mrp_archive.c
    src/formats/mrp_guest_index.c
    src/formats/gwy_cfg.c
    src/formats/gwy_pack_registry.c
    src/formats/mrp_reg_primary.c
    src/profiles/compat_profile.c
    src/runtime/ext_resolver.c
    src/runtime/module_registry.c
    src/runtime/ext_loader.c
    src/runtime/gwy_ext_obs.c
    src/runtime/handler_forensic.c
    src/runtime/guest_call_observer.c
    src/runtime/ext_entry_observe.c
    src/runtime/ext_chunk_observe.c
    src/runtime/ext_object_observe.c
    src/runtime/ext_helper_handoff.c
    src/runtime/ext_dsm_record_observe.c
    src/runtime/ext_module_entry_abi.c
    src/runtime/ext_entry_null_contract.c
    src/runtime/ext_module_data_init.c
    src/runtime/ext_er_rw_producer.c
    src/runtime/ext_bootstrap_abi.c
    src/runtime/ext_callback_frame.c
    src/runtime/ext_r9_scope_audit.c
    src/runtime/ext_post_cont_audit.c
    src/runtime/ext_post_cfn_r9_audit.c
    src/runtime/ext_p_extchunk_audit.c
    src/runtime/ext_shell_publication_audit.c
    src/runtime/ext_mrpgcmap_entry_order.c
    src/runtime/ext_entry_abi_cluster_audit.c
    src/runtime/ext_cfunction_publication_audit.c
    src/runtime/ext_chunk_provider.c
    src/runtime/ext_er_rw_bind_restore.c
    src/runtime/ext_gwy_startgame_audit.c
    src/runtime/module_r9_switch.c
    src/runtime/mrp_member_view.c
    src/runtime/package_scope.c
    src/runtime/package_metadata.c
    src/runtime/vm_runtime.c
    src/runtime/guest_memory.c
    src/runtime/vm_file_service.c
    src/runtime/gwy_vmrp_vfs_bootstrap.c
    src/platform/platform_identity.c
    src/platform/platform_userinfo.c
    src/platform/platform_send_app_event.c
    src/platform/platform_timer.c
    src/platform/platform_handler_registry.c
    src/platform/platform_call_census.c
)

set(GWY_RESEARCH_GWY_SHELL_SOURCES
    src/runtime/robotol_idle_watch.c
    src/runtime/robotol_flag_writer_trace.c
    src/runtime/jjfb_bmp_meta.c
    src/runtime/jjfb_plat_11f00.c
    src/runtime/ext_gwy_shell_shim.c
    src/runtime/ext_gwy_shell_native_exec.c
    src/runtime/e10a_shell_trace.c
    src/runtime/e10a3_postselect_trace.c
    src/runtime/e10a31_gamelist_context.c
    src/runtime/e10a31a_precont_diag.c
    src/runtime/e10a31b_publication.c
    src/runtime/e10a31c_dispatch.c
    src/runtime/e10a31d_provenance.c
    src/runtime/e10a31e_appinfo.c
    src/runtime/e10a31f_failsite.c
    src/runtime/e10a31g_strcmp.c
    src/runtime/e10a31h_smscfg.c
    src/runtime/e10a31j_smscfg_long.c
    src/runtime/e10a31l_config_map.c
    src/runtime/e10a31m_fail_2e5404.c
    src/runtime/e10a31n_post_range.c
    src/runtime/sms_cfg_compat_profile.c
    src/runtime/gwy_sms_cfg.c
)

add_library(launcher_core STATIC ${GWY_LAUNCHER_CORE_SOURCES})
target_include_directories(launcher_core PUBLIC include "${GWY_UNICORN_ROOT}/include")
target_link_libraries(launcher_core PUBLIC ZLIB::ZLIB "${GWY_UNICORN_ROOT}/unicorn.lib")
target_compile_definitions(launcher_core PUBLIC GWY_HAVE_UNICORN=1)
if(WIN32)
  target_link_libraries(launcher_core PUBLIC user32)
endif()
target_compile_options(launcher_core PRIVATE -Wall -Wextra -Wpedantic)

# Research-track probes (E10A / GWY shell / SMSCFG diagnostics). Not product default.
add_library(research_gwy_shell STATIC ${GWY_RESEARCH_GWY_SHELL_SOURCES})
target_include_directories(research_gwy_shell PUBLIC include "${GWY_UNICORN_ROOT}/include")
target_link_libraries(research_gwy_shell PUBLIC ZLIB::ZLIB "${GWY_UNICORN_ROOT}/unicorn.lib")
target_compile_definitions(research_gwy_shell PUBLIC GWY_HAVE_UNICORN=1)
target_compile_options(research_gwy_shell PRIVATE -Wall -Wextra -Wpedantic)
if(WIN32)
  target_link_libraries(research_gwy_shell PUBLIC user32)
endif()

# Noop stubs so product binaries link without research implementations.
add_library(research_gwy_shell_stubs STATIC
    src/research/gwy_shell/research_gwy_shell_stubs.c
)
target_include_directories(research_gwy_shell_stubs PUBLIC include)
target_compile_options(research_gwy_shell_stubs PRIVATE -Wall -Wno-unused-parameter)

# Product default: core + stubs. Research runners: core + research_gwy_shell.
option(GWY_LINK_RESEARCH_GWY_SHELL "Link real research_gwy_shell instead of stubs" OFF)
if(GWY_LINK_RESEARCH_GWY_SHELL)
  set(GWY_RUNTIME_RESEARCH_LIB research_gwy_shell)
else()
  set(GWY_RUNTIME_RESEARCH_LIB research_gwy_shell_stubs)
endif()

function(gwy_link_product_runtime target)
  target_link_libraries(${target} PRIVATE
    -Wl,--start-group
    launcher_core
    ${GWY_RUNTIME_RESEARCH_LIB}
    -Wl,--end-group
  )
endfunction()

function(gwy_link_research_runtime target)
  target_link_libraries(${target} PRIVATE
    -Wl,--start-group
    launcher_core
    research_gwy_shell
    -Wl,--end-group
  )
endfunction()

# Unicorn DLL must sit beside PE32 test/launcher binaries.
function(gwy_copy_unicorn_dll target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${GWY_UNICORN_ROOT}/unicorn.dll"
      "$<TARGET_FILE_DIR:${target}>/unicorn.dll"
    COMMENT "Copy unicorn.dll for ${target}")
endfunction()

add_executable(gwy_launcher
    src/app/main.c
    src/app/games_picker_win32.c
)
gwy_link_product_runtime(gwy_launcher)
if(WIN32)
  target_link_libraries(gwy_launcher PRIVATE user32 gdi32 comctl32)
endif()
target_compile_options(gwy_launcher PRIVATE -Wall -Wextra -Wpedantic)
gwy_write_build_info(gwy_launcher)
gwy_copy_unicorn_dll(gwy_launcher)

enable_testing()

add_executable(test_smoke tests/unit/test_smoke.c)
gwy_link_product_runtime(test_smoke)
add_test(NAME smoke COMMAND test_smoke)

add_executable(test_mrp_jjfb tests/unit/test_mrp_jjfb.c)
gwy_link_product_runtime(test_mrp_jjfb)
add_test(NAME mrp_jjfb COMMAND test_mrp_jjfb)

add_executable(test_cfg36 tests/unit/test_cfg36.c)
gwy_link_product_runtime(test_cfg36)
add_test(NAME cfg36 COMMAND test_cfg36)

add_executable(test_game_catalog tests/unit/test_game_catalog.c)
gwy_link_product_runtime(test_game_catalog)
add_test(NAME game_catalog COMMAND test_game_catalog)

add_executable(test_guest_vfs tests/unit/test_guest_vfs.c)
gwy_link_product_runtime(test_guest_vfs)
add_test(NAME guest_vfs COMMAND test_guest_vfs)

add_executable(test_launch_descriptor tests/unit/test_launch_descriptor.c)
gwy_link_product_runtime(test_launch_descriptor)
add_test(NAME launch_descriptor COMMAND test_launch_descriptor)

add_executable(test_ext_resolver tests/unit/test_ext_resolver.c)
gwy_link_product_runtime(test_ext_resolver)
add_test(NAME ext_resolver COMMAND test_ext_resolver)

add_executable(test_module_registry tests/unit/test_module_registry.c)
gwy_link_product_runtime(test_module_registry)
add_test(NAME module_registry COMMAND test_module_registry)

add_executable(test_ext_loader tests/unit/test_ext_loader.c)
gwy_link_product_runtime(test_ext_loader)
add_test(NAME ext_loader COMMAND test_ext_loader)

add_executable(test_mrp_member_view tests/unit/test_mrp_member_view.c)
gwy_link_product_runtime(test_mrp_member_view)
add_test(NAME mrp_member_view COMMAND test_mrp_member_view)

add_executable(test_mrp_reg_primary tests/unit/test_mrp_reg_primary.c)
gwy_link_product_runtime(test_mrp_reg_primary)
add_test(NAME mrp_reg_primary COMMAND test_mrp_reg_primary)

add_executable(test_package_scope tests/unit/test_package_scope.c)
gwy_link_product_runtime(test_package_scope)
add_test(NAME package_scope COMMAND test_package_scope)

add_executable(test_package_metadata tests/unit/test_package_metadata.c)
gwy_link_product_runtime(test_package_metadata)
add_test(NAME package_metadata COMMAND test_package_metadata)

add_executable(test_ext_entry_observe tests/unit/test_ext_entry_observe.c)
gwy_link_product_runtime(test_ext_entry_observe)
add_test(NAME ext_entry_observe COMMAND test_ext_entry_observe)

add_executable(test_ext_chunk_observe tests/unit/test_ext_chunk_observe.c)
gwy_link_product_runtime(test_ext_chunk_observe)
add_test(NAME ext_chunk_observe COMMAND test_ext_chunk_observe)

add_executable(test_ext_object_observe tests/unit/test_ext_object_observe.c)
gwy_link_product_runtime(test_ext_object_observe)
add_test(NAME ext_object_observe COMMAND test_ext_object_observe)

add_executable(test_ext_helper_handoff tests/unit/test_ext_helper_handoff.c)
gwy_link_product_runtime(test_ext_helper_handoff)
add_test(NAME ext_helper_handoff COMMAND test_ext_helper_handoff)

add_executable(test_ext_dsm_record_observe tests/unit/test_ext_dsm_record_observe.c)
gwy_link_product_runtime(test_ext_dsm_record_observe)
add_test(NAME ext_dsm_record_observe COMMAND test_ext_dsm_record_observe)

add_executable(test_ext_module_entry_abi tests/unit/test_ext_module_entry_abi.c)
gwy_link_product_runtime(test_ext_module_entry_abi)
add_test(NAME ext_module_entry_abi COMMAND test_ext_module_entry_abi)

add_executable(test_ext_entry_null_contract tests/unit/test_ext_entry_null_contract.c)
gwy_link_product_runtime(test_ext_entry_null_contract)
add_test(NAME ext_entry_null_contract COMMAND test_ext_entry_null_contract)

add_executable(test_ext_module_data_init tests/unit/test_ext_module_data_init.c)
gwy_link_product_runtime(test_ext_module_data_init)
gwy_copy_unicorn_dll(test_ext_module_data_init)
add_test(NAME ext_module_data_init COMMAND test_ext_module_data_init)

add_executable(test_module_r9_switch tests/unit/test_module_r9_switch.c)
gwy_link_product_runtime(test_module_r9_switch)
gwy_copy_unicorn_dll(test_module_r9_switch)
add_test(NAME module_r9_switch COMMAND test_module_r9_switch)

add_executable(test_ext_er_rw_producer tests/unit/test_ext_er_rw_producer.c)
gwy_link_product_runtime(test_ext_er_rw_producer)
gwy_copy_unicorn_dll(test_ext_er_rw_producer)
add_test(NAME ext_er_rw_producer COMMAND test_ext_er_rw_producer)

add_executable(test_ext_bootstrap_abi tests/unit/test_ext_bootstrap_abi.c)
gwy_link_product_runtime(test_ext_bootstrap_abi)
gwy_copy_unicorn_dll(test_ext_bootstrap_abi)
add_test(NAME ext_bootstrap_abi COMMAND test_ext_bootstrap_abi)

add_executable(test_ext_callback_frame tests/unit/test_ext_callback_frame.c)
gwy_link_product_runtime(test_ext_callback_frame)
gwy_copy_unicorn_dll(test_ext_callback_frame)
add_test(NAME ext_callback_frame COMMAND test_ext_callback_frame)

add_executable(test_ext_post_cont_audit tests/unit/test_ext_post_cont_audit.c)
gwy_link_product_runtime(test_ext_post_cont_audit)
gwy_copy_unicorn_dll(test_ext_post_cont_audit)
add_test(NAME ext_post_cont_audit COMMAND test_ext_post_cont_audit)

add_executable(test_ext_post_cfn_r9_audit tests/unit/test_ext_post_cfn_r9_audit.c)
gwy_link_product_runtime(test_ext_post_cfn_r9_audit)
gwy_copy_unicorn_dll(test_ext_post_cfn_r9_audit)
add_test(NAME ext_post_cfn_r9_audit COMMAND test_ext_post_cfn_r9_audit)

add_executable(test_ext_p_extchunk_audit tests/unit/test_ext_p_extchunk_audit.c)
gwy_link_product_runtime(test_ext_p_extchunk_audit)
gwy_copy_unicorn_dll(test_ext_p_extchunk_audit)
add_test(NAME ext_p_extchunk_audit COMMAND test_ext_p_extchunk_audit)

add_executable(test_ext_shell_publication_audit tests/unit/test_ext_shell_publication_audit.c)
gwy_link_product_runtime(test_ext_shell_publication_audit)
gwy_copy_unicorn_dll(test_ext_shell_publication_audit)
add_test(NAME ext_shell_publication_audit COMMAND test_ext_shell_publication_audit)

add_executable(test_ext_mrpgcmap_entry_order tests/unit/test_ext_mrpgcmap_entry_order.c)
gwy_link_product_runtime(test_ext_mrpgcmap_entry_order)
gwy_copy_unicorn_dll(test_ext_mrpgcmap_entry_order)
add_test(NAME ext_mrpgcmap_entry_order COMMAND test_ext_mrpgcmap_entry_order)

add_executable(test_ext_entry_abi_cluster_audit tests/unit/test_ext_entry_abi_cluster_audit.c)
gwy_link_product_runtime(test_ext_entry_abi_cluster_audit)
gwy_copy_unicorn_dll(test_ext_entry_abi_cluster_audit)
add_test(NAME ext_entry_abi_cluster_audit COMMAND test_ext_entry_abi_cluster_audit)

add_executable(test_ext_cfunction_publication_audit tests/unit/test_ext_cfunction_publication_audit.c)
gwy_link_product_runtime(test_ext_cfunction_publication_audit)
gwy_copy_unicorn_dll(test_ext_cfunction_publication_audit)
add_test(NAME ext_cfunction_publication_audit COMMAND test_ext_cfunction_publication_audit)

add_executable(test_ext_chunk_provider tests/unit/test_ext_chunk_provider.c)
gwy_link_product_runtime(test_ext_chunk_provider)
gwy_copy_unicorn_dll(test_ext_chunk_provider)
add_test(NAME ext_chunk_provider COMMAND test_ext_chunk_provider)

add_executable(test_ext_er_rw_bind_restore tests/unit/test_ext_er_rw_bind_restore.c)
gwy_link_product_runtime(test_ext_er_rw_bind_restore)
gwy_copy_unicorn_dll(test_ext_er_rw_bind_restore)
add_test(NAME ext_er_rw_bind_restore COMMAND test_ext_er_rw_bind_restore)

add_executable(test_ext_gwy_startgame_audit tests/unit/test_ext_gwy_startgame_audit.c)
gwy_link_product_runtime(test_ext_gwy_startgame_audit)
gwy_copy_unicorn_dll(test_ext_gwy_startgame_audit)
add_test(NAME ext_gwy_startgame_audit COMMAND test_ext_gwy_startgame_audit)

add_executable(test_ext_gwy_shell_shim tests/unit/test_ext_gwy_shell_shim.c)
gwy_link_research_runtime(test_ext_gwy_shell_shim)
add_test(NAME ext_gwy_shell_shim COMMAND test_ext_gwy_shell_shim)

add_executable(test_platform_identity tests/unit/test_platform_identity.c)
gwy_link_product_runtime(test_platform_identity)
add_test(NAME platform_identity COMMAND test_platform_identity)

add_executable(test_platform_userinfo tests/unit/test_platform_userinfo.c)
gwy_link_product_runtime(test_platform_userinfo)
add_test(NAME platform_userinfo COMMAND test_platform_userinfo)

add_executable(test_platform_timer tests/unit/test_platform_timer.c)
gwy_link_product_runtime(test_platform_timer)
add_test(NAME platform_timer COMMAND test_platform_timer)

add_executable(test_platform_send_app_event tests/unit/test_platform_send_app_event.c)
gwy_link_product_runtime(test_platform_send_app_event)
add_test(NAME platform_send_app_event COMMAND test_platform_send_app_event)

add_executable(test_platform_handler_registry tests/unit/test_platform_handler_registry.c)
gwy_link_product_runtime(test_platform_handler_registry)
add_test(NAME platform_handler_registry COMMAND test_platform_handler_registry)

add_executable(test_vm_runtime tests/unit/test_vm_runtime.c)
gwy_link_product_runtime(test_vm_runtime)
gwy_copy_unicorn_dll(test_vm_runtime)
add_test(NAME vm_runtime COMMAND test_vm_runtime)

add_executable(test_guest_memory tests/unit/test_guest_memory.c)
gwy_link_product_runtime(test_guest_memory)
gwy_copy_unicorn_dll(test_guest_memory)
add_test(NAME guest_memory COMMAND test_guest_memory)

add_executable(test_vm_file_service tests/unit/test_vm_file_service.c)
gwy_link_product_runtime(test_vm_file_service)
add_test(NAME vm_file_service COMMAND test_vm_file_service)

set_tests_properties(mrp_jjfb cfg36 game_catalog launch_descriptor ext_resolver mrp_member_view PROPERTIES ENVIRONMENT "GWY_FIXTURE_ROOT=")
