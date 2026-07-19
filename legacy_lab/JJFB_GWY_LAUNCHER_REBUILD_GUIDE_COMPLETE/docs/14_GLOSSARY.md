# 术语表

- **MRP**：Mythroad 应用容器，包含 `start.mr`、EXT、资源等。
- **EXT**：ARM/Thumb native 模块，不是 Lua 文本。
- **start.mr**：MRP 入口脚本/字节码。
- **reg.ext**：模块注册相关 native 数据/代码，常可见主 EXT 与子模块名称。
- **GWY**：冒泡网游外壳/游戏列表资源域。
- **Launcher Shim**：不运行失效外壳 UI，直接复刻其启动职责的兼容层。
- **LaunchDescriptor**：从 cfg/profile/MRP 生成的不可变启动描述。
- **VFS**：guest 路径到 host canonical/overlay 的映射。
- **Platform ABI**：Mythroad 应用调用的文件、内存、显示、timer、event、network 等接口。
- **PlatformRegistry**：guest 主动注册的 handler/callback/timer 表。
- **Scheduler**：按照通用生命周期、时间和输入调度已注册 guest callback。
- **ERW**：EXT 运行时读写区。其内部偏移属于游戏实现细节，不应进入 launcher core。
- **legacy_lab**：保存旧固定地址实验和日志的只读区域。
- **compatibility profile**：声明目标资源、参数和有限兼容规则的 JSON；不是内存补丁脚本。
