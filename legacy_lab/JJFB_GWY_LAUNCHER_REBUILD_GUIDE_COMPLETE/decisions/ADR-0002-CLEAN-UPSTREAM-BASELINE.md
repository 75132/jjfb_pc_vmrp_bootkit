# ADR-0002：从 upstream clean baseline 新建工程

- 状态：Accepted
- 日期：2026-07-18

当前修改版 bridge 已混合大量版本实验、固定地址和状态注入。继续拆洗无法证明残留行为，也会让 Cursor 误把实验当架构。

决策：冻结旧 bootkit 为 legacy evidence，从 `runtime/vmrp_src/vmrp-master` 建 clean repo，只按模块迁移经过测试的通用能力。
