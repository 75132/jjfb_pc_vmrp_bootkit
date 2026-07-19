# Clean 仓库布局与核心 API 契约

## 1. 推荐目录（可直接创建）

```text
gwy-launcher/
├─ CMakeLists.txt
├─ cmake/
├─ README.md
├─ LICENSES/
├─ third_party/
│  └─ vmrp_upstream/                 # 固定版本，只做小型适配 patch
├─ include/gwy_launcher/
│  ├─ error.h
│  ├─ mrp_archive.h
│  ├─ gwy_cfg.h
│  ├─ launch_descriptor.h
│  ├─ launch_profile.h
│  ├─ guest_vfs.h
│  ├─ guest_memory.h
│  ├─ vm_runtime.h
│  ├─ ext_resolver.h
│  ├─ ext_loader.h
│  ├─ platform_call.h
│  ├─ platform_registry.h
│  ├─ platform_scheduler.h
│  └─ trace.h
├─ src/
│  ├─ app/
│  ├─ formats/
│  ├─ launcher/
│  ├─ vfs/
│  ├─ runtime/
│  ├─ platform/
│  ├─ profiles/
│  └─ trace/
├─ profiles/
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  ├─ fixtures/
│  └─ golden/
├─ tools/
├─ docs/
└─ legacy_lab/                        # 可选只读链接；不参与 build
```

## 2. 错误模型

不要到处返回模糊 `-1`。建议：

```c
typedef enum LauncherStatus {
    L_OK = 0,
    L_ERR_INVALID_ARGUMENT,
    L_ERR_IO,
    L_ERR_FORMAT,
    L_ERR_BOUNDS,
    L_ERR_HASH_MISMATCH,
    L_ERR_PROFILE_MISMATCH,
    L_ERR_NOT_FOUND,
    L_ERR_UNSUPPORTED,
    L_ERR_GUEST_FAULT,
    L_ERR_DECOMPRESSION,
    L_ERR_STATE,
    L_ERR_NETWORK
} LauncherStatus;

typedef struct LauncherError {
    LauncherStatus code;
    const char *subsystem;
    char message[256];
    char detail[512];
} LauncherError;
```

规则：

- 库函数不直接 `exit()`；
- error message 带路径/member/code；
- trace 记录机器可读 code；
- 不因为 compatibility profile 自动吞掉格式错误。

## 3. MRP Archive API

```c
typedef struct MrpArchive MrpArchive;
typedef struct MrpMember {
    char name[256];
    uint32_t offset;
    uint32_t stored_size;
    uint32_t reserved;
    bool looks_gzip;
} MrpMember;

LauncherStatus mrp_archive_open(
    const char *path,
    MrpArchive **out,
    LauncherError *err);

LauncherStatus mrp_archive_find_exact(
    const MrpArchive *archive,
    const char *member_name,
    const MrpMember **out,
    LauncherError *err);

LauncherStatus mrp_archive_decode_member(
    const MrpArchive *archive,
    const MrpMember *member,
    size_t max_decoded_size,
    ByteBuffer *out,
    LauncherError *err);
```

不允许 `find` 自动做 alias。alias 是 `ExtResolver` 的职责。

## 4. cfg API

```c
typedef struct GwyCfgRecord {
    uint32_t index;
    ByteView raw;
    OptionalString title;
    OptionalString icon;
    OptionalU32 napptype;
    OptionalU32 nextid;
    OptionalU32 ncode;
    OptionalString target_mrp;
    FieldEvidence fields[...];
} GwyCfgRecord;
```

每个字段不仅有值，还要带：

```text
offset
length
encoding
confidence
source layout version
```

这样未来发现 cfg 变体时不会破坏旧布局。

## 5. LaunchDescriptor

```c
typedef struct LaunchDescriptor {
    char profile_id[64];
    char resource_root[PATH_MAX];
    char target_mrp[256];
    char entry_member[64];
    uint32_t cfg_index;
    int32_t napptype, nextid, ncode, narg, narg1;
    char nmrpname[256];
    char flags[8][32];
    uint32_t appid, appver;
    uint8_t target_sha256[32];
    DisplaySpec display;
    IdentitySpec identity;
} LaunchDescriptor;
```

构造后只读。运行期状态放 `LaunchContext`，不能回写 descriptor。

## 6. VFS API

```c
typedef enum VfsBackend {
    VFS_CANONICAL_READONLY,
    VFS_OVERLAY_WRITABLE,
    VFS_GENERATED
} VfsBackend;

typedef struct VfsResolution {
    char guest_original[512];
    char guest_normalized[512];
    char host_path[PATH_MAX];
    VfsBackend backend;
    char rule[64];
} VfsResolution;

LauncherStatus guest_vfs_resolve(
    GuestVfs *vfs,
    const char *guest_path,
    VfsOpenMode mode,
    VfsResolution *out,
    LauncherError *err);
```

`VFS_GENERATED` 可提供 `sdk_key.dat` 等平台生成文件，但生成来源、hash 和 lifetime 必须记录。

## 7. EXT resolver API

```c
typedef enum ResolveReason {
    RESOLVE_EXACT,
    RESOLVE_REG_PRIMARY,
    RESOLVE_PROFILE_ALIAS
} ResolveReason;

typedef struct ResolvedExt {
    char requested_name[128];
    char resolved_name[128];
    ResolveReason reason;
    const MrpMember *member;
} ResolvedExt;
```

解析顺序固定：

```text
exact →可信 reg.ext primary→ profile alias → fail
```

profile alias 不得覆盖一个已存在的 exact member。

## 8. PlatformCall API

```c
typedef struct PlatformCall {
    uint32_t code;
    GuestPtr input;
    uint32_t input_len;
    GuestPtr output;
    uint32_t output_capacity;
    GuestPtr caller_pc;
    GuestPtr owner_ext;
} PlatformCall;

typedef struct PlatformCallResult {
    int32_t guest_return;
    uint32_t output_len;
    bool deferred_work_enqueued;
} PlatformCallResult;
```

服务函数只能通过 `GuestMemory` 读写参数。

## 9. Registry API

```c
typedef enum HandlerKind {
    HANDLER_FAMILY,
    HANDLER_PERIODIC,
    HANDLER_ENQUEUE,
    HANDLER_CALLBACK
} HandlerKind;

typedef struct RegisteredHandler {
    HandlerKind kind;
    GuestPtr entry;
    GuestPtr context;
    uint64_t owner_generation;
    bool active;
} RegisteredHandler;
```

不把某个 observed address 写进源码；地址由 guest 注册产生。

## 10. Scheduler API

```c
typedef enum ScheduledEventKind {
    EVT_LIFECYCLE,
    EVT_TIMER,
    EVT_INPUT,
    EVT_NETWORK,
    EVT_GUEST_DEFERRED
} ScheduledEventKind;

LauncherStatus scheduler_enqueue(...);
LauncherStatus scheduler_drain_one(...);
void scheduler_on_guest_enter(...);
void scheduler_on_guest_leave(...);
```

强制不变量：

```text
guest_depth > 0 时 scheduler_drain_one 不得进入 guest
```

## 11. Trace schema 最低字段

```json
{
  "seq": 123,
  "mono_us": 456789,
  "subsystem": "ext_resolver",
  "event": "member_resolved",
  "launch_id": "...",
  "profile_id": "jjfb",
  "guest_depth": 0,
  "data": {}
}
```

日志是证据接口，不是散乱 printf。保留简洁文本镜像供人工阅读。
