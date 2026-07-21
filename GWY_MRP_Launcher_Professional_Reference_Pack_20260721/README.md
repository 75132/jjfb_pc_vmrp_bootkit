# GWY / MRP 公用启动器专业参考资料包

本包是对实际挂载的 240x320 Mythroad/GWY 运行树进行的只读全量解析，面向 `jjfb_pc_vmrp_bootkit` 及同类公用启动器开发。

## 建议阅读顺序

1. `01_GWY_MRP_Launcher_Professional_Reference_20260721.docx`：正式专业报告；
2. `01_GWY_MRP_Launcher_Professional_Reference_20260721.md`：便于检索、Git diff；
3. `02_GWY_Launcher_Engineering_Action_Plan.md`：直接用于工程实施；
4. `03_Data_Dictionary.md`：CSV 字段/用途说明；
5. `data/`：全量扫描数据；
6. `tools/gwy_full_reference_scan.py`：stdlib-only 可复跑扫描器；
7. `figures/`：架构、双轨和层次边界图。

## 本次实扫摘要

- 文件：1601；约 26.57 MiB；
- MRP：134/134 成功解析，错误 0；
- MRP 成员：11514；
- EXT：77；
- 顶层 `gwy/*.mrp`：45；
- side/resource pack：65；
- 尺寸化资源名：606；
- start.mr 家族：8；
- `gwy.mrp`、`jjfb.mrp`、`wxjwq.mrp` 共用同一个 `mrc_loader.ext`。

## 边界

- 不包含原始 MRP/EXT 字节；
- 不修改任何游戏文件；
- 不把 FAST/DEBUG/固定地址实验当作产品成功；
- 扫描输出含哈希、成员元数据、字符串与推导模型，便于复现和审计。
