# 参考实现计划：从 CLI 到原始 MRP 运行

本文给 Cursor 一个接近代码级的实现顺序，但不提供目标固定地址。

## 1. 主程序流程

```c
int main(int argc, char **argv) {
    CliOptions cli;
    LauncherError err = {0};
    LaunchProfile profile;
    LaunchDescriptor desc;
    LaunchContext ctx;

    CHECK(cli_parse(argc, argv, &cli, &err));
    CHECK(profile_load_and_validate(cli.profile_path, &profile, &err));
    CHECK(launch_descriptor_build(&profile, &desc, &err));
    CHECK(launch_descriptor_validate_resources(&desc, &err));
    CHECK(trace_open(cli.trace_path, &ctx.trace, &err));
    CHECK(launch_manifest_write(&desc, &ctx.trace, &err));

    CHECK(guest_vfs_create(&desc, cli.user_data_root, &ctx.vfs, &err));
    CHECK(platform_create(&desc, &ctx.vfs, &ctx.platform, &err));
    CHECK(vm_runtime_create(&ctx.platform, &ctx.vm, &err));
    CHECK(ext_resolver_create(&profile, &ctx.resolver, &err));
    CHECK(scheduler_create(&ctx.vm, &ctx.platform.registry,
                           &ctx.scheduler, &err));

    CHECK(launch_state_transition(&ctx, STATE_PLATFORM_READY, &err));
    CHECK(start_mr_run(&ctx, &desc, &err));
    CHECK(launch_accept_entry_result(&ctx, &profile, &err));
    CHECK(launch_state_transition(&ctx, STATE_EXT_INITIALIZED, &err));

    while (!ctx.stop_requested) {
        platform_poll_host(&ctx.platform);
        scheduler_enqueue_due(&ctx.scheduler);
        scheduler_drain_budget(&ctx.scheduler, 1);
        platform_present_if_dirty(&ctx.platform);
        scheduler_sleep_until_deadline(&ctx.scheduler);
    }

    launch_shutdown(&ctx);
    return 0;
}
```

`CHECK` 不能掩盖错误；失败时写 structured summary 并执行幂等 cleanup。

## 2. Descriptor build 伪代码

```text
profile.expected.cfg_index
→ cfg_open(profile.cfg_path)
→ cfg_get_record(index)
→ decode known fields
→ locate target under canonical root
→ mrp_open
→ read appid/appver/hash/member index
→ compare all expected values
→ serialize launch parameter
→ freeze descriptor
```

profile 是断言，不是随意覆盖器：cfg 说 target A、profile 说 B 时必须报 mismatch。

## 3. VFS resolution 规则

建议配置化的顺序：

```text
1. reject NUL, drive, UNC, absolute, traversal
2. normalize slash and repeated separators
3. classify writable path
4. check writable overlay (for read)
5. map canonical forms:
   mythroad/<rel> → resolution_root/<rel>
   gwy/<rel>      → resolution_root/gwy/<rel>
   <rel>          → controlled current-MRP context only
6. generated files (sdk_key etc.)
7. fail with trace
```

不要再依赖进程当前工作目录碰巧找到文件。

## 4. SDK key service

输入：

```text
vmver
IMEI
manufacturer
model
algorithm_version
```

输出：48 bytes binary + SHA256 + generated VFS node。  
JJFB golden identity/key 已放 profile/evidence。算法实现要有独立 CLI/单测。

## 5. StartMrRunner

职责：

- 设置 current archive/current MRP name；
- 传入 descriptor parameter；
- 执行 `start.mr`；
- 捕获 entry return 和 loader milestones；
- 不自行调用游戏内部固定入口。

返回：

```c
typedef struct EntryResult {
    int32_t guest_return;
    bool entry_loaded;
    bool main_ext_resolved;
    bool main_ext_mapped;
    bool main_ext_registered;
    bool init_called;
    int32_t init_return;
} EntryResult;
```

## 6. EXT resolve/load

```text
requested logical name
→ exact archive lookup
→ optional trusted reg.ext primary
→ profile alias after exact miss
→ decode with size cap
→ allocate executable/read-write regions
→ initialize helper/table/context
→ register ExtInstance generation
→ trace
```

JJFB 的 alias 只是数据：

```json
{"requested":"cfunction.ext","resolved":"robotol.ext"}
```

## 7. Platform bootstrap

已验证最低 EXT handshake 可由 profile/capability 描述：

```text
version(6)
appInfo(8)
init(0)
```

但实现必须抽象成 `ExtAbiAdapter`，不能在 launcher main 直接写魔数 switch。每次调用记录 input schema、output、return 和 owner EXT。

## 8. Registry + scheduler

当 guest 通过平台注册 service 提交 handler：

```text
validate Thumb/ARM entry
validate executable guest mapping
store owner ExtInstance generation
replace/clear according to documented semantics
trace registration
```

当 platform service 在 guest 调用中产生工作：

```text
if guest_depth > 0: enqueue only
else: enqueue; outer loop drains
```

退出/重载 EXT 时取消对应 generation 的 handler/timer，防止悬挂地址。

## 9. Display

第一版只做：

- guest framebuffer；
- RGB565 conversion；
- dirty/full present；
- scale/letterbox；
- key/mouse mapping。

不做：资源名识别、OCR、host 文本覆盖、loading 进度条。

## 10. Network

平台 socket API 与业务协议分开：

```text
launcher network backend = socket semantics
JJFB server protocol      = separate future research
```

offline 模式必须返回一致的网络错误，而不是“登录成功”。

## 11. 最小可运行验收 trace

```text
resource.validated
cfg.record_decoded index=36
descriptor.frozen target=gwy/jjfb.mrp
mrp.hash_verified
vfs.open start.mr
vfs.open sdk_key.dat backend=generated/canonical
ext.resolve mrc_loader.ext exact
ext.resolve cfunction.ext -> robotol.ext reason=profile_alias
ext.register robotol.ext
ext.call version ret=0
ext.call appInfo ret=0
ext.call init ret=0
platform.handler_registered ...
launcher.state RUNNING
```

没有 handler 注册时，不要注入；写清是平台 ABI/生命周期待研究。
