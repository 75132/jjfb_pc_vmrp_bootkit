# 数据字典

本目录所有数据均由 `tools/gwy_full_reference_scan.py` 从原始运行树只读生成；交付包不包含原始 MRP/EXT 字节。

| 文件 | 内容 | 主要用途 |
|---|---|---|
| `all_files.csv` | 1601 个文件的相对路径、大小、SHA-256 | 运行树基线、CI 变更检测 |
| `directory_stats.csv` | 目录级文件数、大小、扩展名分布 | 资源根与 target-local layout |
| `mrp_archives.csv` | 134 个 MRP 的头字段、分类、primary、hash | 启动分类、包身份 |
| `mrp_members.csv` | 11514 个成员的 offset、存储/解码长度、codec、hash | member resolver、边界审计 |
| `ext_modules.csv` | 77 个 EXT 的 MRPGCMAP、长度、hash | image mapping、跨目标对照 |
| `interesting_strings.csv` | 模块、路径、URL、资源、平台调用等字符串 | 静态 xref 入口 |
| `dependency_edges.csv` | source→target 的模块/路径/URL 依赖边 | 架构图、缺失依赖审计 |
| `bootstrap_hash_groups.csv` | start.mr 与 mrc_loader 哈希家族 | package classifier |
| `start_template_families.csv` | 启动模板族解释 | common runtime 适配优先级 |
| `reg_primary_groups.csv` | reg.ext 中 primary/模块组 | package-scoped primary |
| `resource_name_grammar.csv` | `name!W!H[@pack].ext` 解析结果 | side-pack resolver |
| `side_pack_registry.csv` | target-local side/resource pack 与 `.v` 配对 | pack inventory/cache 契约 |
| `cfg_variant_inventory.csv` | embedded seed 与 loose runtime cfg 关系 | cfg 版本检测 |
| `cfg_path_hits.csv` | cfg 中所有 MRP path hit 及上下文字节 | schema 反推 |
| `cfg_records_parser_model.csv` | 当前 1024/272 模型的逐记录输出 | 只能作为 PARSER_MODEL |
| `cfg_parser_model_metrics.json` | 模型有效率/异常统计 | schema confidence |
| `control_target_matrix.csv` | root/shell/service/loader/direct 等正控 | 多目标验收 |
| `common_profile_boundaries.csv` | common/profile/diagnostic 边界 | 防止 JJFB 特例污染 |
| `latest_evidence_status.csv` | E9V-E10A 最新运行证据状态 | 静态/动态合并判断 |
| `implementation_roadmap.csv` | A-H 阶段路线与退出条件 | 研发排期 |
| `parse_errors.csv` | 解析错误 | 本次为 0 条 |
