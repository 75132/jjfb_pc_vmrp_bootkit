# GWY / MRP 公用启动器工程行动方案

**版本：2026-07-21**  
**依据：全量只读扫描 1601 个文件、134 个 MRP、11514 个成员、77 个 EXT，以及 E9V-E10A 最新运行证据。**

## 1. 工程目标

产品目标不是复刻某一个游戏的 splash，也不是恢复已经失效的旧在线大厅本身，而是建立一个可复用的 Mythroad/GWY runtime：

```text
原始运行树扫描
→ 版本化 cfg/descriptor 解析
→ package-scoped MRP/EXT 启动
→ P/extChunk、ER_RW/R9、entry/callback ABI
→ VFS、allocator、timer/event、graphics、side-pack
→ 原始目标 MRP 自身逻辑
```

原生 shell 兼容作为并行研究轨：

```text
gwy.mrp 根平台
→ cfunction 模块装配
→ gbrwcore named services
→ gamelist external cfg/update
→ lib.startGame/lib.runapp
→ target MRP
```

两条路线必须共享同一个 common runtime。

## 2. 立即采用的架构边界

### Common runtime

必须公用：

- MRPG 只读解析、边界校验、gzip/zlib/raw 解码；
- archive/member/package identity；
- package-scoped member view、nested load stack；
- `reg.ext` 第一个有效 EXT 作为 primary；
- MRPGCMAP 映像、image+8 candidate、Thumb/ARM、entry/continuation 区分；
- P/extChunk、ER_RW/R9 的可嵌套 context stack；
- VFS、allocator、timer/event/lifecycle、bitmap/text/refresh/input/network primitives；
- target-local side-pack registry 与 `name!W!H@pack.ext` resolver；
- 唯一 `run_id` 和里程碑式 verdict。

### App descriptor/profile

只允许携带最小启动数据：

- cfg schema id/confidence；
- target MRP、图标、标题；
- napptype/nextid/ncode/narg/narg1；
- source label（例如 `descriptor_launcher`）；
- target-local resource roots。

### Shell compatibility profile

仅包含：

- gbrwcore named service dispatcher；
- gamelist cfg/update contract；
- 在观察到真实请求与 callback ABI 后提供 offline/no-update；
- native `lib.startGame/lib.runapp` 研究链。

### Diagnostic profile

仅限断点、trace、地址实验和 FAST assist。`AC8/BD0/C9D` 等 robotol 地址不得进入 common/product 默认路径。

## 3. P0：测试基础设施先修正

1. 每次运行创建唯一 `run_id`；所有 stdout、CSV、截图、verdict 携带同一 id。
2. 每个 case 开始前轮换或清空对应输出；verdict 只能消费当前 run_id。
3. verdict 必须由里程碑 gate 产生：没有命中 target splash，禁止输出 `*_AC8_*_AT_SPLASH`。
4. 固化统一里程碑：

```text
ARCHIVE_OPEN
→ START_MR
→ PRIMARY_RESOLVED
→ EXT_IMAGE_MAPPED
→ EXT_ENTRY / CALLBACK_CONTINUATION
→ P_EXTCHUNK
→ ER_RW_R9
→ INIT_RET
→ HANDLER_REGISTER
→ EVENT_FIRE
→ FIRST_RESOURCE
→ FIRST_DRAW
→ FIRST_REFRESH
→ STABLE_IDLE / NETWORK_WAIT
```

## 4. P1：实现 `GwyPackageContext`

建议最小结构：

```c
typedef struct GwyPackageContext {
    uint64_t archive_id;
    uint64_t member_view_id;
    uint64_t current_module_id;
    char archive_path[260];
    char primary_member[96];
    char vfs_root[260];

    uint32_t P;
    uint32_t extChunk;
    uint32_t ER_RW;
    uint32_t R9;
    uint32_t entry_pc;
    uint32_t continuation_pc;
    uint32_t cpsr;
    uint32_t callback_sp;

    int entry_kind;      /* image_entry / callback / secondary_entry */
    int source_kind;     /* root / descriptor / native_shell */
    void *launch_descriptor;
} GwyPackageContext;
```

所有 c_load、file、timer、event、platform API 必须显式接收或可追溯到该 context。跨模块调用必须 push/pop，不得覆盖单一全局 module/R9。

## 5. P2：先用最小正控钉死 common ABI

按顺序运行：

| 目标 | 作用 | 预期最低里程碑 |
|---|---|---|
| `gwy/roomlist.mrp` | ff67 shell/service 模板、仅 start/reg/primary | primary init |
| `gwy/vdload.mrp` | 同族 + file/network API | primary init + 第一平台请求 |
| `gwy/tlbb.mrp` | e318 游戏模板 + direct primary | `dream.ext` init |
| `gwy/wxjwq.mrp` | 与 JJFB 同 start/mrc_loader | `mmochat.ext` init |
| `gwy/jjfb.mrp` | 复杂 loader/module/side-pack | `robotol.ext` init + 生命周期 |

判读：

- roomlist/vdload/TLBB 都失败：common package/file/entry ABI 仍错；
- direct-primary 成功、WXJWQ/JJFB 同失败：mrc_loader contract 缺失；
- WXJWQ 成功、JJFB 失败：才进入 robotol/JJFB 特有诊断；
- 非 JJFB 目标绝不能加载 JJFB 地址 profile。

## 6. P3：版本化 cfg detector

当前 1024 base + 272 record size 只是 `PARSER_MODEL`。必须：

1. 先枚举 `gwy/*.mrp` 路径命中；
2. 在路径窗口测试候选 schema；
3. 检查 target 是否存在、icon 后缀、UTF-16 可打印性、数值范围；
4. 输出 `schema_id`、`confidence`、字段来源 offset、record hash；
5. 只对高置信记录生成 descriptor；
6. 支持 embedded 6898-byte seed 与 loose 20728-byte runtime cfg；
7. JJFB 当前记录标题以 `0x58` 可完整得到“机甲风暴(火爆公测)”，但不得未经验证推广为全局常量。

## 7. P4：side-pack/resource resolver 产品化

统一语法：

```text
name!W!H.ext
name!W!H@pack.ext
```

解析流程：

```text
logical name parse
→ target-local pack registry
→ guest index natural match
→ sibling/postmatch
→ archive exact fallback
→ original bytes + hash
→ bitmap/audio/data decoder
```

禁止写死 `show1`、`downimage1`、`jjfbol`。资源根应从目标 package/root inventory 推导。

## 8. P5：文件 API 上下文必须在平台边界捕获

GuestVFS 只处理规范路径，不负责猜寄存器。每次 file API 入站前记录：

```text
package/module/P/extChunk/ER_RW/R9
PC/LR/SP/CPSR/R0-R7
R0-R3 作为字符串、pointer-to-pointer、descriptor 的候选
SP+0/+4/+8/+C
[R0+0/+4/+8/+C/+10]
```

输出候选分类：valid_path、control_byte、descriptor、pointer_to_pointer、unmapped、non_string。由具体 API ABI adapter 选择参数，再交给统一 VFS。

## 9. P6：根包基线不能省略

`gwy.mrp` 不是普通游戏包。其 `cfunction.ext` 引用 reglogin、dload、gui、rollscr、gamelist、resmng、vdload、pmsg、embfrd、svrctrl、font；其 `mrc_loader.ext` 与 JJFB/WXJWQ 相同。

研究轨至少需要比较：

- 从 `gwy.mrp` 根包进入；
- 从 `gbrwcore_mr_start` 直接进入；
- 从 descriptor launcher 进入目标游戏；

三者在 registry、P/extChunk、ER_RW/R9、平台表、参数与 VFS root 上的差异。

## 10. 交付验收

公用启动器最低验收矩阵：

- root：`gwy.mrp` 完成 loader + cfunction/graphics context；
- service：roomlist/vdload primary init；
- shell：gamelist 至少真实读取外部 cfg，研究轨再推进 native runapp；
- mrc_loader：WXJWQ、JJFB 使用同一 loader，分别进入 primary；
- direct-primary：TLBB 进入 dream.ext；
- resource-heavy：spacetime 能完成 member lookup 和首次 draw；
- large EXT：sanguo 完成大映像、callback/lifecycle。

任何目标成功都必须保留原 MRP/EXT hash，不使用 host overlay，不伪造游戏网络成功。
