# 给 Cursor 的最短入口

1. 打开本仓库根目录。
2. 把 [`CURSOR_MASTER_PROMPT.md`](CURSOR_MASTER_PROMPT.md) 全文发给 Cursor。
3. 先读 [`../00_READ_ME_FIRST.md`](../00_READ_ME_FIRST.md) 与 [`../REPO_LAYOUT.md`](../REPO_LAYOUT.md)。
4. 产品入口只使用根目录 `RUN_BUILD.ps1` / `RUN_TESTS.ps1` / `RUN_PRODUCT_DIRECT_JJFB.ps1`。
5. 研究 runner 在 `research/runners/`，不要塞回根目录。
6. 每轮跑 `tools/audit_launcher_core.py`。

路线口令：

> 解析 shell，重建 launch contract；模拟平台，不扶游戏状态。
