# Mythroad 平台 ABI 与通用 Scheduler 设计

## 1. ABI 服务分类

### 基础内存

- guest memory map；
- malloc/free；
- cache sync；
- guest pointer validity；
- EXT code/data mapping；
- 防止 host pointer 泄漏到 guest。

### 文件与 VFS

- open/close/read/write/seek/len；
- mkdir/remove/rename/info/opendir；
- canonical + overlay；
- MRP archive member read；
- 安全路径规范化；
- 结构化 file trace。

### identity/time

- vmver；
- IMEI；
- manufacturer/model；
- datetime/uptime；
- sdk_key generation；
- user-info blob（字段未完全确定时版本化）。

### display/input

- guest RGB565 framebuffer；
- 240×320 逻辑坐标；
- SDL 320×480 stretch；
- DrawBitmap/DrawRect/DispUpEx/DrawChar；
- dirty rect/present；
- 键盘/鼠标映射到通用 MRP 事件。

禁止在 display backend 识别 `slogo`、`loadingbar` 名称并代画。

### timer/event

- timer registry；
- monotonic clock；
- due queue；
- non-reentrant guest callback dispatch；
- deferred events；
- pause/resume 时钟语义；
- input event queue。

### network

- init/close；
- socket/connect/send/recv；
- DNS；
- async state；
- trace；
- 可选 offline policy（返回明确错误，不伪造游戏成功）。

## 2. 平台调用注册表

`sendAppEvent`/扩展调用不能继续是单个 JJFB 巨型 switch。建议：

```c
typedef int32_t (*PlatformServiceFn)(PlatformCall *call, PlatformContext *ctx);

typedef struct {
    uint32_t code;
    const char *name;
    PlatformServiceFn fn;
    PlatformCallPolicy policy;
} PlatformServiceEntry;
```

每个 service：

- 校验参数；
- 记录输入/输出；
- 更新 `PlatformRegistry`；
- 不读取 JJFB ERW offset；
- 不调用固定 guest PC。

## 3. 已观察注册类调用的处理原则

### `0x10102`

观察为 family handler 注册。应保存：

```text
family id
handler guest address
ownership/ext chunk
```

### `0x10140`

观察为周期/主 handler 注册。保存 handler 和 cadence/类型字段，但不要假设地址 `0x30630D`。

### `0x10165`

观察为 enqueue handler 注册。平台事件入队时调用注册 handler；不直接跳到旧实验链。

### `0x10120`

保存 ext chunk/handler context。

### `0x10113`

用于提供平台图形函数指针。应由 `platform_table` 返回通用 bridge stub。

### `0x10180`

设备/用户信息服务。需要用结构版本和长度严格校验，不要返回随意 blob。

## 4. 非重入调度

当前实验发现 guest 在 EXT 调用中产生 family/event 时容易嵌套 emulation。正确 scheduler：

```text
Guest call depth > 0
→ service 只 enqueue
→ 当前 guest call 返回
→ scheduler drain queue
→ 进入下一个 guest callback
```

伪代码：

```c
while (runtime.running) {
    poll_host_input();
    enqueue_due_timers();
    if (runtime.guest_depth == 0) {
        dispatch_one_platform_event();
    }
    present_if_dirty();
    sleep_until_next_deadline();
}
```

这是一项可迁移的通用能力，不是 game-state 注入。

## 5. 生命周期事件来源

事件只能来自：

- 启动器状态机；
- host OS input/window；
- timer deadline；
- network completion；
- guest 主动注册/请求；
- replay trace。

事件不能来自：

- “B71 还是 0，所以发 C0”；
- “ui_mode 没变，所以发 event 5”；
- “某固定 PC 没执行，所以调用它”。

## 6. capability negotiation

平台对未知 code 应：

1. 记录 code、参数、guest caller；
2. 返回文档化的 unsupported/failed；
3. 不默认返回成功；
4. 允许通过 profile 开启经过验证的 generic capability；
5. 用其他 MRP 做横向验证。

## 7. ABI 测试策略

- 单元测试每个 service 的参数校验；
- fake guest pointer 测试越界；
- scheduler 测试非重入；
- timer deterministic clock；
- VFS path traversal；
- network async state；
- display pitch/stride/colorkey；
- 注册→事件→callback 的最小测试 MRP/fixture。
