# 尚未锁定的问题与正确实验方法

## 1. GWY 的真实 runapp/startGame API 到底是什么

当前 `bridge_dsm_mr_start_dsm` 只是最低层等价入口。需要从 `gbrwcore.ext/gamelist.ext` 继续回答：

- 哪个 function 读取 cfg record；
- 参数字符串在哪里生成；
- 最终调用哪项 Mythroad API；
- 启动前是否设置 current MRP/name/root；
- 是否触发 foreground/resume；
- 是否注册返回/退出 handler。

实验：静态 disassembly + platform call trace + 多游戏对照。不要运行强制更新 UI 作为日常路径。

## 2. `reg.ext` 的精确结构

目前通过 strings 能看到模块列表，但还没完全定义二进制结构。

实验：

- 对多个 MRP 的 reg.ext 做结构对齐；
- 比较 module count/name table/function pointer；
- 用 Capstone 标注加载逻辑；
- 输出 versioned parser，无法解析时只退回 strings heuristic。

## 3. `cfunction.ext` 逻辑名的来源

可能性：

- start.mr/loader 使用固定逻辑主模块名；
- reg.ext 提供主模块映射；
- 某些平台版本在 archive lookup 时做别名；
- 目标包构建工具更名。

正确实验：分析多个使用同类 `mrc_loader.ext` 的 MRP。不要继续用 guest literal patch 作为正式答案。

## 4. 生命周期 app/code 的标准语义

当前观察到 app=2/3/5/7/9 等以及 family handler，但历史实验对 C0 的追踪混入 JJFB 结果反推。

正确实验：

- 查看 Mythroad 开发文档/API header；
- 运行简单 MRP fixture；
- 对多个原始 MRP 比较初始化调用；
- trace GWY shell 正常进入其他可运行模块的生命周期；
- 建立事件语义表，标注 confidence/source。

## 5. `0x101xx` 平台调用语义

当前仅有行为观察。应建立：

```text
code
argument schema
return schema
ownership
lifetime
reentrancy
source evidence
confidence
```

任何未知 code 不应默认成功。

## 6. identity/user-info blob

`0x10180` 等调用需要真实结构。当前 host blob 属于兼容实验，需要：

- 从 SDK 头文件/文档搜索结构；
- 比较 caller 读取偏移；
- 使用长度和 version；
- 未确认字段填 0，不填随机内容。

## 7. 网络阶段

launcher 成功后，游戏服务器可能仍离线。需要单独判断：

- 游戏是否只需旧端口服务；
- 是否有本地单机资源路径；
- 是否需要协议模拟；
- 是否涉及账号/服务端逻辑。

这不应提前混入启动器核心。
