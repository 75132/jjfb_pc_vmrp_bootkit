# third_party provenance

## vmrp_upstream

- Source snapshot: former `runtime/vmrp_src/vmrp-master`
- Role: clean Mythroad/vmrp baseline for the independent launcher
- `bridge.c` size at import: 51718 bytes
- Must NOT be replaced by `legacy_lab/runtime/vmrp_src_build_v27` modified sources

## Locked binary deps

See `DEPS.lock.json`.

| Dep | Version | Path |
|---|---|---|
| SDL2 | 2.0.10 | `deps/SDL2-2.0.10` (junction → vmrp_upstream/windows) |
| Unicorn | 1.0.2-win32 | `deps/unicorn-1.0.2-win32` |

These are SDK binaries only. They are not a license to copy modified `bridge.c` into clean core.
