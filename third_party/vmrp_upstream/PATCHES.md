# Upstream adaptation patches

Only tiny build-blocking fixes belong here. Do not import JJFB hooks from legacy_lab.

## 0001-vmrp-printf-string

- File: `vmrp.c`
- Issue: broken multi-line `printf` string (missing `\n` / unterminated literal) prevents clean compile
- Fix: single-line `printf(...\n", ...)`
- Classification: PLATFORM/build hygiene (not game-state)

## 0002-gwy-launch-env

- File: `vmrp.c`
- Issue: default entry always starts `dsm_gm.mrp` (GWY shell / update path)
- Fix: when `GWY_LAUNCH=1`, start `GWY_LAUNCH_TARGET` (default `gwy/jjfb.mrp`) with cfg36 param
- Classification: DESCRIPTOR / launch contract (no fixed guest addresses)

## 0003-gwy-vm-file-bind

- Files: `fileLib.c`, `vmrp.c`, `header/gwy_vm_file_abi.h`, `gwy_vm_file_weak.c`
- Issue: dual filesystem (cwd host open vs GuestVFS)
- Fix: when `gwy_vm_file_is_bound()`, route open/read/write/seek/close/getLen/info through VmFileService ABI; no silent host fallback while bound. `GWY_LAUNCH` calls `gwy_vmrp_prepare_guest_vfs()` before load. Weak stubs keep unbound builds working.
- Classification: PLATFORM / VFS adapter (no game-state)

## 0004-gwy-runtime-binding

- Files: `Makefile`, `vmrp.c`, `gwy_vm_file_weak.c`
- Issue: default `make main` could remain unbound; GWY_LAUNCH continued unbound on prepare failure; MinGW PE does not honor ELF weak stubs reliably
- Fix: dual targets `plain` (strong unbound stubs) vs `gwy` (link `liblauncher_core.a`, no stub.o). `GWY_LAUNCH=1` fail-fast if unbound. Formal scripts deploy Gwy to `out/vmrp_run`.
- Classification: PLATFORM / launch contract

## 0005-mrp-member-view

- Files: launcher_core `mrp_member_view.c` + `gwy_vmrp_vfs_bootstrap.c` (linked into Gwy runtime)
- Issue: jjfb.mrp exact-misses member `cfunction.ext`; logical member is `robotol.ext`. Guest DSM `_mr_readFile` scans package index and cannot see clean-core ExtResolver.
- Fix: after GuestVFS bind, load CompatibilityProfile; on strong match (target/appid/appver/sha256), install a **generated index view** that adds MRP member name `cfunction.ext` pointing at `robotol.ext` stored blob. Original resource-tree `jjfb.mrp` bytes/hash unchanged. DSM host-file `cfunction.ext` (loadCode) is never aliased.
- Classification: EXT / MRP_MEMBER scope only (no GuestVFS global rename, no guest string patch)

## 0006-ext-loader-lifecycle-obs

- Files: `bridge.c` (`br__mr_c_function_new`, `br_mr_malloc`, `bridge_ext_init`, `bridge_mr_extHelper`), `header/gwy_ext_obs_abi.h`, `gwy_ext_obs_weak.c` (plain only), launcher_core `ext_loader.c` / `gwy_ext_obs.c` / bootstrap
- Issue: ModuleRegistry stopped at EXTRACTED; no load_id association from real map/register/entry
- Fix: observe upstream loader events into ExtLoadSession/ModuleRegistry (EXTRACTED→MAPPED→REGISTERED→ENTRY_CALLED). No second loader, no helper ordinal, no JJFB fixed addresses. DSM recorded as `dsm:cfunction.ext` separate from MRP member alias.
- Classification: EXT observability (PLATFORM thin hooks)
