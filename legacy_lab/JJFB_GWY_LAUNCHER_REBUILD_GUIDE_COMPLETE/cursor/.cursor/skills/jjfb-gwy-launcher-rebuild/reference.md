# Quick Reference

## Product path

```text
ResourceDiscovery
→ GwyCfgParser
→ LaunchDescriptorBuilder
→ LaunchValidator
→ GuestVfs
→ VmRuntime
→ StartMrRunner
→ ExtResolver
→ ExtLoader
→ PlatformRegistry
→ PlatformScheduler
```

## Research path

```text
gbrwcore/gbrwshell/gamelist/vdload static inspection
→ trace shell launch calls
→ compare multiple original games
→ write protocol evidence
→ only then promote generic behavior into product core
```

The research path produces evidence. It is never a runtime dependency of the launcher.

## Immediate red flags

- `ERW + ...`
- absolute guest PC in core
- `FORCE_*`
- code conditional on JJFB resource names
- fake splash/loading/login
- auto-success for unknown platform call
- nested guest emulation from inside a bridge callback
- writes to source resource tree
