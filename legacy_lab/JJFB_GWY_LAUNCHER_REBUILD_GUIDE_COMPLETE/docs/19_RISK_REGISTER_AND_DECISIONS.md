# 风险登记、架构决策与止损线

## 1. 高风险项

| 风险 | 影响 | 早期信号 | 对策 |
|---|---|---|---|
| 继续修改旧 bridge | 再次路线漂移 | 文件持续增长、版本宏堆叠 | 新仓库；legacy 只读 |
| cfg 布局误判 | 启错目标/字段 | 只对 index36 有效 | 多记录/多 cfg 验证，字段带 confidence |
| alias 写成硬编码 patch | 目标绑定 | 出现 ext_base+offset | resolver/profile 层 alias |
| 未知平台 code 默认成功 | 隐蔽状态错误 | 日志“都返回0” | fail/unsupported + trace |
| scheduler 重入 | 崩溃/状态错乱 | callback 内再次 uc_emu_start | deferred queue + depth guard |
| source tree 被写 | 污染原资源 | save/cache 出现在 gwy | writable overlay |
| 以画面作为成功标准 | 诱发 host 假 UI | screenshot 变成主要证据 | loader/registry/scheduler 验收 |
| 把离线服务器问题混入启动器 | 无穷 mock | init 已成功仍改 loader | network/business 独立里程碑 |
| Cursor 一次改太多 | 难以审计 | 一轮跨 3 个 Phase | 单任务、测试、停下汇报 |

## 2. 已作架构决策

### ADR-001：日常启动不运行旧 GWY shell

使用 cfg/profile 直接构建 descriptor；shell 仅研究。理由：强制更新是失效外壳功能，不是目标 MRP 的必要组成。

### ADR-002：从 upstream clean baseline 重建

不拆洗现有修改 bridge。理由：变更规模和耦合已无法可靠证明通用性。

### ADR-003：原始目标 MRP 不修改

兼容差异由 profile/resolver/platform 实现。理由：可复现、可验证、避免把 bypass patch 当平台实现。

### ADR-004：scheduler 是核心，不是补丁集合

所有 lifecycle/timer/event/network completion 进入一个非重入队列。理由：当前大量实验问题本质上来自缺失/嵌套的平台调度。

### ADR-005：JJFB 只是第一个 integration profile

核心命名和接口面向 GWY/MRP，JJFB 仅是黄金目标。至少再选两个简单 MRP 做 cross-target 测试。

## 3. 立即停止条件

遇到以下任一情况，Cursor 必须停止当前实现并写 blocker report：

- 唯一方案需要写目标固定地址；
- 需要改 MRP 文件才能继续；
- 不知道平台调用参数却准备返回成功；
- 为了“让画面动”准备注入未证实事件；
- 一次提交修改超过三个核心子系统；
- 测试无法区分方案 A/B；
- target hash 变化；
- source resource tree 有写入。

## 4. 成功衡量

第一阶段成功不是“进入游戏大厅”，而是：

```text
clean launcher 可扫描多个 GWY 游戏
→ descriptor 可验证
→ 原始目标 loader 完整
→ 平台注册可捕获
→ scheduler 可运行
→ 核心无 target-state forcing
```

网络业务和旧服务器恢复属于后续独立项目。
