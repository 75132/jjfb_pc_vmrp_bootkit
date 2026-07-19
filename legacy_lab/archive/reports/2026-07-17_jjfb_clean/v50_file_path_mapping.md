# v50 文件路径映射表（静态预期）

> 这是 runner 执行前的静态映射基线。运行后由 `scripts/v50_analyze_launcher_log.py` 生成实际打开结果。

| guest path | host resolved path | exists? | 预期说明 |
|---|---|---:|---|
| `gwy/jjfb.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfb.mrp` | 是 | 主机资源 |
| `mythroad/240x320/gwy/jjfb.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfb.mrp` | 是 | 主机资源 |
| `mythroad/gwy/jjfb.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfb.mrp` | 是 | 主机资源 |
| `/gwy/jjfb.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfb.mrp` | 是 | 主机资源 |
| `gwy/jjfbol/default.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfbol\default.mrp` | 是 | 主机资源 |
| `jjfbol/default.mrp` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfbol\default.mrp` | 是 | 主机资源 |
| `gwy/gifs/ng_jjfb.gif` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\gifs\ng_jjfb.gif` | 是 | 主机资源 |
| `gifs/ng_jjfb.gif` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\gifs\ng_jjfb.gif` | 是 | 主机资源 |
| `gwy/save` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\save` | 是 | 主机资源 |
| `save` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\save` | 是 | 主机资源 |
| `gwy/sound/msg.mid` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\sound\msg.mid` | 是 | 主机资源 |
| `sound/msg.mid` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\sound\msg.mid` | 是 | 主机资源 |
| `cfg.bin` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\cfg.bin` | 是 | 主机资源 |
| `mrc_loader.ext` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\mrc_loader.ext` | 否 | 若为 MRP 内部项，应由 MRP loader 读取，不应主机直开 |
| `robotol.ext` | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\robotol.ext` | 否 | 若为 MRP 内部项，应由 MRP loader 读取，不应主机直开 |

## 映射优先级

1. 开启 `JJFB_GWY_LAUNCHER_MODE=1` 后，`mythroad/240x320/*`、`240x320/*`、`mythroad/gwy/*`、`gwy/*` 优先映射到 canonical `mythroad/240x320`。
2. 无前缀资源（如 `jjfbol/default.mrp`、`gifs/ng_jjfb.gif`、`save/*`）优先映射到 `gwy_root`。
3. canonical 不存在时才回退进程当前目录，防止旧的扁平副本掩盖路径错误。
4. 创建文件时，即使目标尚不存在，也返回 canonical 路径，以保证缓存/存档写回 `gwy/save` 等真实目录。

## 运行时待填字段

runner 日志需补齐：`opened by pc/lr`、`handle/ret`、`FILEOPEN_MISS` 次数和首个真正阻断点。
