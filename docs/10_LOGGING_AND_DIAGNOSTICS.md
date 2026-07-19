# 日志与诊断规范

## 1. 结构化 JSONL 优先

每条事件字段：

```json
{
  "ts_ms": 1234,
  "level": "info",
  "subsystem": "ext_loader",
  "event": "member_resolved",
  "profile": "gwy.jjfb.original",
  "guest_path": "gwy/jjfb.mrp",
  "member": "robotol.ext",
  "source": "profile_alias",
  "stored_length": 161178,
  "decoded_length": 253420
}
```

终端可输出简短人类日志，但证据以 JSONL 为准。

## 2. 推荐 subsystem

```text
launcher
resource
cfg
mrp
vfs
vm
ext_loader
platform_call
registry
scheduler
timer
event
display
network
storage
blocker
```

## 3. 必须记录的启动事件

- profile/schema validation；
- resource/cfg/target hashes；
- descriptor；
- VFS resolve；
- MRP member lookup；
- SDK identity/key hash；
- EXT map/register；
- method calls/returns；
- platform registration；
- scheduler transitions；
- network endpoints；
- exit reason。

## 4. blocker 分类

```text
RESOURCE_MISSING
CFG_LAYOUT_UNKNOWN
TARGET_HASH_MISMATCH
MRP_INVALID
MEMBER_NOT_FOUND
EXT_DECODE_FAILED
EXT_REGISTER_FAILED
ABI_UNSUPPORTED
GUEST_POINTER_INVALID
LIFECYCLE_UNRESOLVED
CALLBACK_NOT_REGISTERED
NETWORK_UNAVAILABLE
GUEST_EXCEPTION
```

不要写笼统的“游戏没起来”。

## 5. 日志去噪

默认禁止：

- 每字符日志；
- 每像素/每 blit；
- 每条 ARM 指令；
- 不限量寄存器 dump；
- 固定地址覆盖探针；
- 版本号前缀无限叠加。

支持 `--trace-level` 和 subsystem filter。

## 6. Trace/replay

注册和调度事件应可导出：

```text
platform_call_received
handler_registered
timer_created
event_enqueued
callback_dispatched
callback_returned
```

replay 用于测试 scheduler，不用于伪造游戏服务器或业务结果。
