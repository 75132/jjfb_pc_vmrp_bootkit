# Cursor 继续开发说明：v29 — sendAppEvent / timer / module-network 路线

> 最新状态：`mrc_loader.ext -> robotol.ext -> mrc_init` 已经跑通，`host mrc_init(0) ret=0` 已确认。  
> 现在不要再改 sdk_key、不要盲跳 start.mr、不要回头修编译环境。下一阶段是把 `sendAppEvent(+0x28)` 做成更接近真机的平台消息实现，并让 timer/event 主循环真正驱动 robotol 继续加载 module / network。

---

## 1. 当前已确认突破

已经打通：

```text
start.mr
  ↓
sdk_key line143/147 错误分支跳过
  ↓
_strCom(601, "mrc_loader.ext")
  ↓
_strCom(800, mrc_loader_buf, 0)
  ↓
mrc_loader.ext 加载
  ↓
_strCom(801, "", 1)
  ↓
mrc_loader 读 robotol.ext，返回 {ptr,len}
  ↓
第二次 _strCom(800, {ptr,len}, 0)
  ↓
robotol.ext 完整装入
  ↓
host 补 C 路径 6 → 8 → 0
  ↓
robotol mrc_init(0) ret=0
```

关键日志：

```text
logs/v27_host_init_extchunk_stdout.txt
logs/v27_host_init_timer_stdout.txt
logs/PROGRESS_mrc_init_ok.md
```

验证点：

```text
mr_get_method(161178)
mr_cacheSync(..., 253504)
_mr_c_function_new(00304AE5, 20)
P@0x2AC8D4 = {ER_RW=0x2B1850 len=5404 type=1 chunk=0x0 stack=0x0}
synthesized mrc_extChunk @0x2829DC -> P+0xc
host mrc_init(0) ret=0
```

---

## 2. 当前卡点

虽然 `mrc_init=0`，但仍未出现：

```text
initNetwork
module.ext
持续 20000 / 21002 / 6009
```

当前新问题：

```text
mr_timer event unexpected!
```

以及 `sendAppEvent` 仍是启发式 stub：

```c
if (a1 >= 0x100 && a1 < 0x800000) malloc(a1);
else return 1;
```

这只能让 init 过关，不代表平台语义正确。

---

## 3. 下一步不要做的事

不要做：

```text
1. 不要再猜 sdk_key.dat。
2. 不要再把 mrc_loader.ext 当 extName 直接启动。
3. 不要继续跳 start.mr 行号。
4. 不要回头补 windows/unicorn / SDL2 / PATH。
5. 不要用 UCRT64 64-bit 混编。
6. 不要把 sendAppEvent 简单固定 return 1/0。
```

---

## 4. 当前应修文件

重点文件：

```text
runtime/vmrp_src_build_v27/vmrp-master/bridge.c
runtime/vmrp_src_build_v27/vmrp-master/vmrp.c
runtime/vmrp_src_build_v27/vmrp-master/header/bridge.h
runtime/vmrp_src_build_v27/vmrp-master/doc/反汇编研究.c
runtime/vmrp_src_build_v27/vmrp-master/doc/整理的ext重要函数.c
```

---

## 5. sendAppEvent 真实语义线索

文档签名：

```c
typedef int32 (*mrc_extMainSendAppMsg_t)(
    int32 extCode,
    int32 app,
    int32 code,
    int32 param0,
    int32 param1
);
```

`mrc_extChunk_st` 关键字段：

```c
0x00 check = 0x7FD854EB
0x04 init_func
0x08 event / helper
0x0c code_buf
0x10 code_len
0x14 var_buf
0x18 var_len
0x1c global_p_buf
0x20 global_p_len
0x24 timer
0x28 sendAppEvent
0x2c extMrTable
```

timer 调用参考：

```c
mrc_extTimerStart(param1) {
    chunk = mr_c_function_P->mrc_extChunk;
    chunk->sendAppEvent(0, chunk, 0, chunk->timer, param1);
}

mrc_extTimerStop() {
    chunk = mr_c_function_P->mrc_extChunk;
    chunk->sendAppEvent(0, chunk, 1, chunk->timer, 0);
}
```

---

## 6. 已观察到的 sendAppEvent code

`mrc_init` 期间出现：

```text
sendAppEvent code=0x1     a1=100
sendAppEvent code=0x10113 a1=73474
sendAppEvent code=0x10113 a1=73475
sendAppEvent code=0x10113 a1=73476
sendAppEvent code=0x10102 a1=123392 a2=0x30D2F9
sendAppEvent code=0x10120 a1=4     a2=chunk
sendAppEvent code=0x10140 a1=5     a2=chunk a3=0x306305
sendAppEvent code=0x10162 a1=57856 a2=...
sendAppEvent code=0x10165 a1=57856 a2=...
sendAppEvent code=0x10800 a1=4
```

当前 stub 只是把大 `a1` 当 malloc，其余 return 1。  
下一步需要区分：

```text
A. timer start/stop 消息：extCode=0，app=chunk，code=0/1，param0=timer，param1=period
B. platEx/alloc/平台能力消息：0x101xx / 0x10800
```

---

## 7. 建议开发任务

### 任务 1：重新命名 sendAppEvent 参数，别再用 code/a1/a2/a3 模糊命名

当前：

```c
uc_reg_read(R0, &code);
uc_reg_read(R1, &a1);
uc_reg_read(R2, &a2);
uc_reg_read(R3, &a3);
```

建议改成：

```c
uint32_t extCode, app, code, param0, param1;
R0 -> extCode
R1 -> app
R2 -> code
R3 -> param0
SP[0] -> param1
```

因为这是 5 参数函数，当前只读了前 4 个寄存器，**第 5 参数 param1 在栈上**。  
这点很重要，timer start 的 period 可能在第 5 参数。

建议日志：

```c
printf("[JJFB_SEND] extCode=0x%X app=0x%X code=0x%X param0=0x%X param1=0x%X\n",
       extCode, app, code, param0, param1);
```

---

### 任务 2：实现 timer start/stop 语义

当：

```text
extCode == 0
app == jjfb_ext_chunk_addr
code == 0
```

视为 timer start：

```c
chunk->timer = param0 或保留 param0 作为 timer id
period = param1
调用/设置 vmrp 的 timerStart(period)
记录 timer 状态 RUNNING
return 0 或 1（根据真机语义试验）
```

当：

```text
extCode == 0
app == jjfb_ext_chunk_addr
code == 1
```

视为 timer stop：

```c
timerStop(param0)
timer 状态停止
return 0 或 1
```

然后在 `vmrp.c` 主循环里不要只手动打 8 次 `bridge_dsm_ext_call(code=2)`，而是让 timer 状态进入 RUNNING 后，持续触发：

```c
bridge_dsm_ext_call(uc, 2, NULL, 0)
```

并配合 DSM 原本的：

```c
bridge_dsm_mr_timer(uc)
```

避免：

```text
mr_timer event unexpected!
```

---

### 任务 3：保留 host init 路线，但让 timer 驱动后续

当前 host 补：

```text
code 6 -> code 8 -> code 0 -> code 2 x8
```

建议：

```text
code6/version 保留
code8/appInfo 保留
code0/mrc_init 保留
timer 不要固定 8 次，改成循环 30~120 秒持续触发
```

同时记录：

```text
是否有 module.ext 打开
是否有 initNetwork
是否有 mr_connect / mr_socket / net_connect
是否有 20000 / 21002 / 6009
```

---

### 任务 4：实现 0x101xx 平台能力初版

先不要全实现，先做最小可观测：

```text
0x10102：看起来像 malloc / buffer alloc，继续 malloc(a1) 并返回 guest pointer
0x10113：可能是 get/plat query，先记录 a1/a2/a3 指向内容，返回 1
0x10120 / 0x10140：可能是 timer/handler 注册，记录并保存 handler 地址
0x10162 / 0x10165：记录指针内容，尝试 dump string/struct
0x10800：记录，先返回 1
```

建议加 dump：

```c
dump_guest_bytes(a2, 64)
dump_guest_string_if_printable(a2)
dump_guest_bytes(a3, 64)
```

目标是看这些消息是否在注册 handler、模块路径、网络配置。

---

### 任务 5：跟踪文件与网络

在这些位置加日志：

```text
mr_open / mr_read / mr_close
network.c: connect / socket / send / recv
bridge_dsm_network_cb
```

重点文件名：

```text
gwy/jjfbol/
*.module.ext
robotol.ext
mrc_loader.ext
initNetwork
```

重点端口：

```text
20000
21002
6009
```

---

## 8. 成功标准

当前阶段成功不是“窗口不白”，而是日志推进：

最低成功：

```text
mrc_init=0 后不再立即停
timer 状态 RUNNING
不再出现 mr_timer event unexpected
code=2 由 timer 循环持续触发
```

中级成功：

```text
出现 module.ext / initNetwork / 更多 robotol 后续日志
```

高级成功：

```text
netstat 出现 20000 / 21002 / 6009
或 stdout 出现 socket/connect/send/recv
```

---

## 9. 给 Cursor 的一句话总结

**当前已经不是 loader 装入问题，而是 mrc_init 后平台 sendAppEvent / timer / event 语义不完整。请先修 sendAppEvent 的 5 参数读取、timer start/stop、持续 code=2 事件循环，并记录 0x101xx 平台消息内容，直到 module/network 出现。**
