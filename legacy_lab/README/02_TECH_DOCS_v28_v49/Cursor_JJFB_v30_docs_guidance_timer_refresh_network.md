# Cursor 继续开发说明：v30 — 利用新技术文档修 sendAppEvent / timer / refresh / module

> 用户新找了一包 MRP / Mythroad / 插件技术文档。当前最有价值的不是编译依赖，而是：  
> **确认插件模块机制、mrc_init/mrc_event/timer/refresh/network 的平台语义**，用于继续修白屏和 mrc_init 后无网络的问题。

---

## 1. 这包资料里最有用的文件

### A. 斯凯网络化SDK培训-插件技术篇.pdf

位置大致在：

```text
docx/斯凯网络化SDK培训-插件技术篇.pdf
```

重点信息：

```text
1. 插件是一个或多个程序功能组件的独立封装，表现为一个 MRP 包，类似 DLL。
2. 插件由一个或多个模块组成，模块简写为 EXT，每个 ext 有唯一 ID。
3. 主模块负责：
   - 函数池
   - 刷新链
   - 事件链
   - 网络收发单元
   - 全局数据模块
4. mrc_init() 是启动入口。
5. mrc_event() 是虚拟机事件通知函数，包含按键、触屏、应用状态等。
6. mrc_pause / mrc_resume / mrc_exitApp 分别处理挂起、恢复、退出。
7. 模块注册过程：
   xxx.mrp -> reg.ext -> reg.mrc_init() -> 注册模块信息和接口函数 -> xxx.ext 处于注册未加载状态。
8. 模块加载过程：
   调用模块对外接口 -> 主模块调用 mpsFpLoadExt() 加载 xxx.ext -> xxx.mrc_init() 初始化 -> 接口函数被调用。
9. 模块加载可隐式触发；模块函数号大多在 0x10000 以上。
10. 刷新不是每个模块随便刷，而是通过主模块刷新链管理。
11. 事件也不是直接丢给某个模块，而是通过主模块事件链分发。
```

对当前项目的意义：

```text
robotol.ext 很可能就是主模块/函数池/刷新链/事件链的核心之一。
mrc_init=0 只是主模块初始化成功，不代表 UI 和网络已经开始。
接下来必须让事件链、刷新链、timer 和网络收发单元运转。
```

---

### B. 程序编写说明.md

位置大致在：

```text
docx/程序编写说明.md
```

重点信息：

```text
1. 不能使用线程，使用定时器模拟游戏主循环。
2. 绘图结束后必须调用 mrc_refreshScreen，否则屏幕不会显示。
3. mrc_init / mrc_event / mrc_pause / mrc_resume / mrc_exitApp 是程序入口函数。
4. mrc_extRecvAppEvent / mrc_extRecvAppEventEx 是插件调用函数，需要保留。
5. mrc_event:
   - code == KY_UP / KY_DOWN 表示按键事件，param0 为键值。
   - code == MS_UP / MS_DOWN 表示触屏事件，param0 / param1 为坐标。
6. 定时器函数：
   int32 mrc_timerCreate(void);
   void  mrc_timerDelete(int32 t);
   void  mrc_timerStop(int32 t);
   typedef void (*mrc_timerCB)(int32 data);
   int32 mrc_timerStart(int32 t, int32 time, int32 data, mrc_timerCB f, int32 loop);
```

对当前白屏的意义：

```text
白屏不一定代表 loader 错。
如果没有 mrc_refreshScreen，窗口会一直不更新。
如果没有 timer 主循环，游戏逻辑和刷屏函数不会被调用。
```

---

### C. 功能机开发规范.md

位置大致在：

```text
docx/功能机开发规范.md
```

重点信息：

```text
1. 游戏主循环使用 mrc_timerCreate / mrc_timerStart。
2. 常见游戏循环：33ms 一次，相当于 30fps。
3. 定时器回调里执行游戏逻辑、绘图，然后 mrc_refreshScreen。
4. 在 mrc_event 中按当前窗口分发按键/触摸事件。
```

对当前项目的意义：

```text
当前 Cursor 方向 “sendAppEvent + timer + 持续 code=2/event loop” 是对的。
但还要加 refresh/draw 日志，否则看不到 UI 是否真的被绘制。
```

---

### D. mpc库_1.1/mpc.h

位置大致在：

```text
docx/mpc库_1.1/mpc.h
```

重点信息：

这个头文件把大量平台 API 映射出来，尤其有用的是：

```c
#define init            MRC_EXT_INIT
#define exitapp         MRC_EXT_EXIT
#define event           mrc_appEvent
#define pause           mrc_appPause
#define resume          mrc_appResume

#define initnetwork     mrc_initNetwork
#define socket          mrc_socket
#define connect         mrc_connect
#define getsocketstate  mrc_getSocketState
#define send            mrc_send
#define recv            mrc_recv
#define closesocket     mrc_closeSocket
#define closenetwork    mrc_closeNetwork

#define timercreate     mrc_timerCreate
#define timerdel        mrc_timerDelete
#define timerstop       mrc_timerStop
#define timerstart      mrc_timerStart

#define cls             mrc_clearScreen
#define ref             mrc_refreshScreen
#define getscrbuf       w_getScreenBuffer
#define setscrbuf       w_setScreenBuffer
#define getscrsize      mrc_getScreenSize
#define setscrsize      mrc_setScreenSize
```

对当前项目的意义：

```text
1. 网络函数名可以重点 grep：
   mrc_initNetwork / mrc_socket / mrc_connect / mrc_send / mrc_recv
2. 白屏要重点 grep：
   mrc_clearScreen / mrc_refreshScreen / w_getScreenBuffer
3. timer 要重点 grep：
   mrc_timerCreate / mrc_timerStart / mrc_timerStop
```

---

### E. mrc_graphics_20260706/mrc_graphics.c / .h

位置大致在：

```text
docx/mrc_graphics_20260706/mrc_graphics.c
docx/mrc_graphics_20260706/mrc_graphics.h
```

重点信息：

里面大量图形函数都通过：

```c
w_getScreenBuffer()
```

直接操作 16-bit 屏幕缓冲区。

对当前项目的意义：

```text
如果 robotol 后续确实开始画图，但窗口仍白屏，需要检查：
1. w_getScreenBuffer 返回的 guest/host buffer 是否正确。
2. mrc_refreshScreen 是否被调用。
3. SDL 窗口是否把 565 buffer 转成正确像素格式。
```

---

## 2. 对当前路线的判断

当前路线没错，而且这包文档反而证明了路线正确：

```text
mrc_init=0 只是启动成功；
MRP/插件系统依赖 timer + event + refresh 链继续驱动；
白屏大概率是后续 timer/event/refresh 没接上，而不是 loader 路线错。
```

尤其是文档明确说：

```text
不能使用线程，使用定时器模拟游戏主循环。
绘图结束后必须调用 mrc_refreshScreen，否则无法显示。
```

所以现在白屏最该排查：

```text
1. 是否有 timerStart？
2. timer 回调是否被触发？
3. 是否进入 mrc_event / mrc_appEvent？
4. 是否有 clear/draw/refresh？
5. 是否进入 mrc_initNetwork / socket / connect？
```

---

## 3. Cursor 下一步应做的事

### 任务 1：继续修 sendAppEvent 5 参数

sendAppEvent 不是 4 参数，必须读取第 5 参数：

```c
typedef int32 (*mrc_extMainSendAppMsg_t)(
    int32 extCode,
    int32 app,
    int32 code,
    int32 param0,
    int32 param1
);
```

应记录：

```c
[JJFB_SEND] extCode app code param0 param1
```

第 5 参数通常在 guest stack 上。

---

### 任务 2：实现 timerCreate / timerStart / timerStop 的真实映射

根据文档：

```c
int32 mrc_timerCreate(void);
void  mrc_timerDelete(int32 t);
void  mrc_timerStop(int32 t);
typedef void (*mrc_timerCB)(int32 data);
int32 mrc_timerStart(int32 t, int32 time, int32 data, mrc_timerCB f, int32 loop);
```

不要只做：

```text
固定 code=2 打 8 次
```

而要维护 timer 表：

```text
timer_id
period_ms
data
callback/function pointer
loop
running
last_fire_time
```

然后主循环按 period 持续触发。

---

### 任务 3：加 refresh/draw 日志，解释白屏

必须在这些 API 加日志：

```text
w_getScreenBuffer
mrc_clearScreen
mrc_refreshScreen
mrc_drawText
mrc_drawBitmap / bitmapShow
mrc_drawRect / mrc_drawLine
SDL_UpdateTexture / SDL_RenderPresent
```

判断：

```text
A. 没有 draw/refresh：说明还没进入 UI 逻辑，继续修 event/timer。
B. 有 draw/refresh 但白屏：说明 screen buffer / SDL 渲染层有问题。
```

---

### 任务 4：加网络 API 日志

根据 mpc.h，重点 hook/log：

```text
mrc_initNetwork
mrc_socket
mrc_connect
mrc_getSocketState
mrc_send
mrc_recv
mrc_closeSocket
mrc_closeNetwork
```

这些出现前，不要急着做抓包 mock，因为游戏还没真正进入联网层。

---

### 任务 5：理解 0x10000 以上函数号

插件培训文档明确说：

```text
0x10000 以上的代码是函数池固定代码区，这些代码分配给各个模块。
模块函数被调用时，主模块可能隐式加载对应 ext。
```

所以当前看到的：

```text
0x10102
0x10113
0x10120
0x10140
0x10162
0x10165
0x10800
```

不要简单当“未知随机码”。它们可能是函数池/模块接口函数号。  
应按函数池逻辑记录：

```text
func_code
module/ext id
load status
target ext name
registered function address
```

---

## 4. 当前最有价值的调试判断

### 白屏判断

```text
如果没有 mrc_refreshScreen：白屏正常，说明事件/刷新链没起来。
如果有 mrc_refreshScreen 但白屏：查 screen buffer/SDL。
```

### 网络判断

```text
如果没有 mrc_initNetwork：不要抓包，说明还没进入网络模块。
如果出现 mrc_initNetwork / mrc_connect：再开始看 20000/21002/6009。
```

### 模块判断

```text
如果 0x101xx 调用后没有加载 module.ext：函数池/隐式加载逻辑没实现。
如果 module.ext 加载了但不 init：mpsFpLoadExt / mrc_init 调用链还缺。
```

---

## 5. 给 Cursor 的一句话总结

**新资料证明当前路线没错：MRP/插件系统依赖主模块的函数池、事件链、刷新链和 timer 主循环。现在白屏不是回滚信号，而是要继续补 timer/event/refresh/network 平台语义。请优先实现 sendAppEvent 5 参数、timer 表与周期触发、mrc_refreshScreen/draw 日志、mrc_initNetwork/socket 日志，并把 0x101xx 当作函数池/模块接口号分析。**
