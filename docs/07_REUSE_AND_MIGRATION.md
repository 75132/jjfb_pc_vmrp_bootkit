# 现有成果迁移矩阵

## 1. 总原则

不要把当前修改版复制为新项目起点。应从：

```text
runtime/vmrp_src/vmrp-master
```

建立 clean baseline，再按本表迁移。

## 2. 可以直接迁移思路，但需重写成模块

| 能力 | 当前位置 | 新位置 | 注意 |
|---|---|---|---|
| canonical path mapping | `fileLib.c` | `src/vfs/guest_vfs.c` | 加 overlay、禁止逃逸、移除 JJFB 特判 |
| SDK key 生成 | `v51_generate_sdk_key.py` | identity service/tool | 保留算法与测试向量 |
| MRP header/member audit | v52/v53 scripts | `formats/mrp_archive` + tool | 做成通用 parser |
| EXT gzip 解压/映射 | Mythroad + bridge | `runtime/ext_loader` | 不写 guest literal |
| Unicorn runtime | upstream/current | `runtime/vm_runtime` | 保持最小补丁 |
| file/network bridges | `fileLib.c/network.c` | platform modules | typed trace，错误语义 |
| 240×320 RGB565 + SDL stretch | `bridge.c/main.c` | display backend | 不含游戏资源名特判 |
| deferred event idea | v61 | scheduler | 基于 call depth，通用实现 |
| platform registration observations | logs/bridge | registry | 不保留固定 handler 地址 |
| MRP hash guard | runner | launcher validation | 启动前后都校验 |

## 3. 可作为证据保留，但不进入 clean build

- v43–v86 fixed-PC hooks；
- ERW offset maps；
- UI writer/gate/disassembly；
- AC8/progress/B70/B71/B58 路线；
- Path A event 5/12 注入；
- family C0 after B71；
- host skip net/login probe；
- glyph block/host fake text；
- splash-specific draw/present hacks；
- `br_log` 的 `ext_base+0xD4` 字面量改写。

这些放到：

```text
legacy_lab/
  source_snapshot/
  logs/
  reports/
  probes/
```

并从 clean CMake/Makefile 排除。

## 4. 必须重新设计的部分

### `bridge.c`

当前约 377KB，含 200+ 函数和大量版本叠加。不能继续拆补丁。应拆成：

```text
platform_file.c
platform_memory.c
platform_display.c
platform_timer.c
platform_event.c
platform_network.c
platform_registry.c
platform_scheduler.c
ext_loader.c
trace.c
```

### `br_jjfb_sendAppEvent`

改为通用 `platform_dispatch_call()`，服务表注册，不出现 `jjfb` 前缀。

### 当前 runner

当前 PowerShell 同时做：资源复制、key、编译、环境变量、游戏状态实验。新 runner 只做：

```text
validate → build → launch --profile
```

实验变量全部删除。

### member alias

从 guest 内存 patch 改为 resolver 规则：

```text
requested cfunction.ext
→ exact miss
→ reg.ext primary module
→ profile alias robotol.ext
```

## 5. 建议的 Git 迁移步骤

```bash
# 1. 保存现状
git tag legacy-v86-lab

# 2. 从 upstream/baseline 建新分支
git switch --orphan clean-launcher
# 或创建全新仓库并把 upstream vmrp 作为 third_party

# 3. 先复制只读 tools/docs/tests，不复制 modified bridge.c

# 4. 每次迁移一个平台能力并配测试

# 5. 启用 audit_launcher_core.py 作为 CI 门禁
```

## 6. 新核心的命名规则

禁止：

```text
jjfb_hook_*
v75_*
force_*
B70/B71/AC8
2DADC4/2FC418
```

允许：

```text
platform_register_family_handler
scheduler_enqueue_event
ext_resolver_lookup_member
vfs_resolve_guest_path
launcher_build_descriptor
```

JJFB 名称只能出现在：

- profile 文件；
- integration test 名；
- evidence/legacy docs；
- target-specific log field `profile_id`。
