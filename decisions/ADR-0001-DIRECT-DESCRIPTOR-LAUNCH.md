# ADR-0001：日常启动使用 Descriptor Direct Launch

- 状态：Accepted
- 日期：2026-07-18

## 背景

原 GWY/gamelist 外壳包含已失效的强制联网和更新流程。项目目标是独立运行本地原始目标 MRP，而不是复原旧商城/更新 UI。

## 方案

A. 完整模拟 GWY UI、更新服务器、gamelist，再点击游戏。  
B. 解析 cfg 和 shell 证据，直接重建游戏选择后的 launch contract。  
C. 修改目标 MRP 跳过入口逻辑。

## 决策

采用 B。A 仅作为研究轨，C 禁止作为正式方案。

## 理由

- 最短绕过失效更新依赖；
- 保持目标 MRP 原样；
- 可扩展到同一 GWY 资源树的其他游戏；
- 能把平台 ABI 与业务网络清晰分离。
