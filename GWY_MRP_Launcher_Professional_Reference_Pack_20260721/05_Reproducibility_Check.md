# 可复现性校验

使用交付的 `tools/gwy_full_reference_scan.py` 对同一只读运行树重新执行独立扫描，关键输出 SHA-256 逐字节一致：

| 输出 | 结果 | SHA-256 前 12 位 |
|---|---|---|
| `all_files.csv` | 一致 | `5d5574819f73` |
| `mrp_archives.csv` | 一致 | `b7661cba84f3` |
| `mrp_members.csv` | 一致 | `d963eaba282c` |
| `ext_modules.csv` | 一致 | `d358f16b5dea` |
| `bootstrap_hash_groups.csv` | 一致 | `05210a511098` |
| `resource_name_grammar.csv` | 一致 | `a312683d1b6a` |
| `scan_summary.json` | 一致 | `bbb9209f55b0` |

复跑命令：

```bash
python tools/gwy_full_reference_scan.py \
  --root /path/to/mythroad/240x320 \
  --output /path/to/output
```

实测摘要：1601 文件、134/134 MRP 成功解析、11514 成员、77 EXT、0 解析错误。
