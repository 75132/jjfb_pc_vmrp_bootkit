# 原始“冒泡游戏320480/网游/gwy”文件层终极深挖报告
> 目标：尽量在文件层把 GWY shell、gamelist、gbrwcore/gbrwshell、jjfb/wxjwq、资源目录、MRP 容器、EXT 模块、启动模板、字符串/网络/参数证据一次性拆开。此报告不修改任何游戏文件，只做静态解析。
## 0. 执行摘要
- 源 ZIP：`冒泡游戏320480.zip`，ZIP 条目 `2466` 个，未压缩总大小约 `237.2 MiB`。
- `网游/gwy` 下文件 `1551` 个，总大小约 `23.9 MiB`。
- 全 ZIP 中 `.mrp` 数量 `130`，MRPG 可解析 `130`。
- `网游/gwy` 顶层 MRP 数量 `45`。
- 当前调试主线（结合上传记录）：6N 已恢复 P+0x0C/mrc_extChunk；6O 已恢复 gbrwcore ER_RW/R9；6P 已进入 gamelist，但未到 cfg36/post_update/runapp。文件层分析因此优先服务于：gamelist member_view/primary、dsm:cfunction helper ABI、cfg36/no_update/runapp。

### 当前最有价值文件层结论
1. **gamelist.mrp 文件内确有 `gamelist.ext` 和 `reg.ext`，且 `reg.ext` 字符串尾部明确含 `gamelist.ext`**。所以 6P 中 `reg_primary failed package=gwy/gamelist.mrp` 更像 member-view/parser/primary-resolve bug，不是原包缺文件。
2. **gbrwcore/gbrwshell/gamelist 都是 shell native package 模式**：`start.mr + reg.ext + 主 ext`，主 ext 分别为 `gbrwcore.ext`、`gbrwshell.ext`、`gamelist.ext`。
3. **jjfb 与 wxjwq 仍是最佳对照**：二者 `start.mr` 完全同 SHA，`mrc_loader.ext` 完全同 SHA，说明 mrc_loader bootstrap 是跨目标共同契约。
4. **cfg.bin 内可直接定位 JJFB 记录上下文**：`ng_jjfb.gif` 与 `gwy/jjfb.mrp` 邻近，和既有 cfg36 启动参数吻合。
5. **gamelist.ext 内有 cfg/runapp/gwyblink 参数字符串；gbrwcore.ext 有 lib.startGame/lib.runapp；vdload/ext 侧有 download/update 相关字符串**。这些是 native shell 路线的文件层依据。
6. **slot API matrix 目前不是最短路**：文件层能证明 shell/gamelist 主链还没清；结合 6P 的 `SLOT_CALL=0`，应先修 gamelist primary 和 dsm:cfunction helper ABI。

## 1. 产物索引
- `csv/all_zip_entries.csv`
- `csv/gwy_subdir_resource_stats.csv`
- `csv/mrp_summary_all.csv`
- `csv/gwy_top_level_mrps.csv`
- `csv/mrp_member_manifest_all.csv`
- `csv/ext_summary_all.csv`
- `csv/interesting_strings_all.csv`
- `csv/start_mr_hash_groups.csv`
- `csv/mrc_loader_hash_groups.csv`
- `csv/reg_primary_groups.csv`
- `csv/cfg_bin_path_records.csv`
- `strings/*.strings.csv`：重点 MRP/EXT 的完整 ASCII 字符串偏移表。
- `strings/*.interesting.csv`：重点 MRP/EXT 的网络、路径、runapp、gwyblink、cfunction 等关键信息过滤表。
- `focused/*.field_sites.csv` / `*.field_clusters.csv`：重点 EXT 的 Thumb LDR/STR 字段模式扫描，辅助后续 P/extChunk/结构初始化定位。

## 2. `网游/gwy` 目录资源结构
| 子目录/分组 | 文件数 | 大小 MiB | 扩展名分布 |
|---|---:|---:|---|
| `(top)` | 48 | 7.058 | `{".mrp": 45, ".bin": 3}` |
| `assg` | 23 | 4.908 | `{".ypak": 21, "(none)": 2}` |
| `smd` | 7 | 2.264 | `{".cfg": 1, ".map": 1, ".res": 2, ".tes": 2, ".dat": 1}` |
| `xjwq` | 15 | 2.169 | `{".rs": 7, ".mrp": 6, ".bin": 1, ".res": 1}` |
| `xyol` | 459 | 1.861 | `{".pak": 15, ".xyp": 242, ".xym": 13, ".xya": 187, ".xyi": 1, ".xyu": 1}` |
| `gkdxy` | 474 | 1.415 | `{".fs": 2, "(none)": 472}` |
| `jjfbol` | 119 | 0.982 | `{".mrp": 59, ".v": 59, "(none)": 1}` |
| `mhxx` | 80 | 0.692 | `{".actor": 17, ".mrp": 8, ".ani": 1, ".bin": 4, ".a": 2, ".g": 47, ".gl": 1}` |
| `ssjx` | 6 | 0.636 | `{"(none)": 1, ".mrp": 5}` |
| `tlbb` | 61 | 0.582 | `{".actor": 7, ".mrp": 7, ".bin": 26, ".a": 2, ".e": 1, ".g": 17, ".gl": 1}` |
| `sgmj` | 1 | 0.555 | `{".res": 1}` |
| `ajss` | 1 | 0.533 | `{".res": 1}` |
| `yxlm` | 154 | 0.155 | `{".txt": 2, ".png": 130, ".pwd": 14, ".aef": 8}` |
| `gifs` | 88 | 0.079 | `{".gif": 46, ".info": 1, ".idx1": 1, ".idx12": 1, ".idx13": 1, ".idx16": 1, ".idx18": 1, ".idx20": 1, ".idx22": 1, ".idx23": 1, ".idx25": 1, ".idx26": 1, ".idx28": 1, ".idx31": 1, ".idx34": 1, ".idx36": 1, ".idx39": 1, ".idx40": 1, ".idx41": 1, ".idx42": 1, ".idx44": 1, ".idx5": 1, ".idx50": 1, ".idx54": 1, ".idx55": 1, ".idx57": 1, ".idx60": 1, ".idx61": 1, ".idx67": 1, ".idx69": 1, ".idx70": 1, ".idx71": 1, ".idx76": 1, ".idx83": 1, ".idx88": 1, ".idx89": 1, ".idx9": 1, ".idx91": 1, ".idx93": 1, ".idx94": 1, ".idx95": 1, ".idx98": 1, ".idx99": 1}` |
| `caclottery` | 2 | 0.014 | `{".lst": 1, ".gif": 1}` |
| `wm1` | 5 | 0.01 | `{".png": 5}` |
| `caclobby` | 2 | 0.006 | `{".gif": 1, ".ini": 1}` |
| `sanguo` | 1 | 0.003 | `{".sgp": 1}` |
| `sound` | 4 | 0.002 | `{".mid": 4}` |
| `spacetime` | 1 | 0.0 | `{".db": 1}` |

## 3. 顶层 MRP 总览
| MRP | size | entries | class | ext members | reg primary | start hash short |
|---|---:|---:|---|---|---|---|
| `ajss.mrp` | 377064 | 34 | native_ext_package | `reg.ext|ajss.ext` | `ajss.ext` | `e318e580d789` |
| `assg.mrp` | 244412 | 13 | native_ext_package | `reg.ext|assg.ext` | `assg.ext` | `e318e580d789` |
| `bbjq.mrp` | 342790 | 43 | native_ext_package | `tlol.ext|reg.ext` | `tlol.ext` | `e318e580d789` |
| `caccharge.mrp` | 113627 | 41 | native_ext_package | `caccharge.ext|reg.ext` | `caccharge.ext` | `7fc8d5412ac5` |
| `cacddz.mrp` | 89044 | 58 | native_ext_package | `cacddz.ext|reg.ext` | `cacddz.ext` | `7fc8d5412ac5` |
| `cacddzani.mrp` | 52301 | 26 | native_ext_package | `reg.ext|cacddzani.ext` | `cacddzani.ext` | `7fc8d5412ac5` |
| `caclobby.mrp` | 75312 | 37 | native_ext_package | `caclobby.ext|reg.ext` | `caclobby.ext` | `7fc8d5412ac5` |
| `caclottery.mrp` | 155848 | 44 | native_ext_package | `caclottery.ext|reg.ext` | `caclottery.ext` | `7fc8d5412ac5` |
| `cacpublic.mrp` | 49933 | 23 | native_ext_package | `cacpublic.ext|reg.ext` | `cacpublic.ext` | `7fc8d5412ac5` |
| `cardcharge.mrp` | 7241 | 3 | native_ext_package | `reg.ext|cardcharge.ext` | `cardcharge.ext` | `aa518953e324` |
| `directpay.mrp` | 24982 | 11 | native_ext_package | `reg.ext|directpay.ext` | `directpay.ext` | `ff67eea7e6ee` |
| `dload.mrp` | 10797 | 3 | native_ext_package | `reg.ext|download.ext` | `download.ext` | `7fc8d5412ac5` |
| `embfrd.mrp` | 8572 | 3 | native_ext_package | `reg.ext|embfrd.ext` | `embfrd.ext` | `7fc8d5412ac5` |
| `font.mrp` | 116098 | 27 | native_ext_package | `reg.ext|font.ext` | `font.ext` | `343197ddca55` |
| `gamelist.mrp` | 72620 | 26 | gwy_shell_core | `gamelist.ext|reg.ext` | `gamelist.ext` | `ff67eea7e6ee` |
| `gbrwcore.mrp` | 100292 | 3 | gwy_shell_core | `reg.ext|gbrwcore.ext` | `gbrwcore.ext` | `ff67eea7e6ee` |
| `gbrwshell.mrp` | 33152 | 9 | gwy_shell_core | `reg.ext|gbrwshell.ext` | `gbrwshell.ext` | `7fc8d5412ac5` |
| `gkdxy.mrp` | 299820 | 9 | native_ext_package | `reg.ext|gkdxy.ext` | `gkdxy.ext` | `e318e580d789` |
| `gui.mrp` | 32052 | 18 | native_ext_package | `gui.ext|reg.ext` | `gui.ext` | `7fc8d5412ac5` |
| `hotchat.mrp` | 30766 | 17 | native_ext_package | `reg.ext|hotchat.ext` | `hotchat.ext` | `7fc8d5412ac5` |
| `jjfb.mrp` | 414602 | 50 | mrc_loader_game | `mrc_loader.ext|bigworldmapmodule.ext|mainmenumodule.ext|mailmodule.ext|moduletest.ext|chatmodule.ext|gezimodule.ext|taobaomodule.ext|gameattackmodule.ext|monsterstatemodule.ext|itembagshopmodule.ext|viewmanmodule.ext|betmodule.ext|monstermodule.ext|leitaimodule.ext|reg.ext|lianmengmodule.ext|othermodule.ext|robotol.ext|shopmodule.ext|itemhechengmodule.ext` | `robotol.ext` | `c8d664aa7034` |
| `pmsg.mrp` | 15242 | 7 | native_ext_package | `reg.ext|pmsg.ext` | `pmsg.ext` | `ff67eea7e6ee` |
| `reglogin.mrp` | 30992 | 13 | native_ext_package | `reg.ext|reglogin.ext` | `reglogin.ext` | `ff67eea7e6ee` |
| `resmng.mrp` | 14610 | 4 | native_ext_package | `reg.ext|resmng.ext` | `resmng.ext` | `ff67eea7e6ee` |
| `rollscr.mrp` | 23519 | 6 | native_ext_package | `reg.ext|roll.ext` | `roll.ext` | `ff67eea7e6ee` |
| `roomlist.mrp` | 13627 | 3 | native_ext_package | `reg.ext|roomlist.ext` | `roomlist.ext` | `ff67eea7e6ee` |
| `sanguo.mrp` | 638474 | 275 | native_ext_package | `reg.ext|sanguo.ext` | `sanguo.ext` | `5addb579b1e9` |
| `sgmj.mrp` | 412941 | 35 | native_ext_package | `reg.ext|sgmj.ext` | `sgmj.ext` | `e318e580d789` |
| `smd.mrp` | 256866 | 5 | native_ext_package | `reg.ext|smd.ext` | `smd.ext` | `e318e580d789` |
| `smsbase.mrp` | 8915 | 3 | native_ext_package | `reg.ext|smsbase.ext` | `smsbase.ext` | `ff67eea7e6ee` |
| `smscharge.mrp` | 28521 | 3 | native_ext_package | `reg.ext|smscharge.ext` | `smscharge.ext` | `ff67eea7e6ee` |
| `spacetime.mrp` | 488893 | 422 | native_ext_package | `reg.ext|spacetime.ext` | `spacetime.ext` | `e318e580d789` |
| `ssjx.mrp` | 219731 | 123 | native_ext_package | `gameonline.ext|reg.ext` | `gameonline.ext` | `e318e580d789` |
| `svrctrl.mrp` | 11740 | 3 | native_ext_package | `reg.ext|svrctrl.ext` | `svrctrl.ext` | `ff67eea7e6ee` |
| `tlbb.mrp` | 307238 | 18 | native_ext_package | `reg.ext|dream.ext` | `dream.ext` | `e318e580d789` |
| `tyol.mrp` | 480559 | 617 | native_ext_package | `reg.ext|tyol.ext` | `tyol.ext` | `e318e580d789` |
| `vdload.mrp` | 11586 | 3 | native_ext_package | `reg.ext|vdload.ext` | `vdload.ext` | `ff67eea7e6ee` |
| `wapgame.mrp` | 7156 | 3 | native_ext_package | `reg.ext|wapgame.ext` | `wapgame.ext` | `7fc8d5412ac5` |
| `wm2.mrp` | 22626 | 3 | native_ext_package | `reg.ext|wm1_help.ext` | `wm1_help.ext` | `e318e580d789` |
| `wm2pet.mrp` | 34609 | 3 | native_ext_package | `reg.ext|wmpk.ext` | `wmpk.ext` | `e318e580d789` |
| `wxjwq.mrp` | 299067 | 21 | mrc_loader_game | `mrc_loader.ext|reg.ext|mmochat.ext` | `mmochat.ext` | `c8d664aa7034` |
| `xaqd.mrp` | 427604 | 281 | native_ext_package | `reg.ext|wm1.ext` | `wm1.ext` | `e318e580d789` |
| `xyol.mrp` | 324227 | 87 | native_ext_package | `reg.ext|xiyou.ext` | `xiyou.ext` | `e318e580d789` |
| `yxlm.mrp` | 350664 | 160 | native_ext_package | `reg.ext|xqj.ext` | `xqj.ext` | `e318e580d789` |
| `zsol.mrp` | 285598 | 19 | native_ext_package | `reg.ext|dream.ext` | `dream.ext` | `e318e580d789` |

## 4. 启动模板与跨游戏对照
### 4.1 start.mr hash 分组（只列 count>1 或重点）
- `e318e580d789041a` count=95: `ajss.mrp, assg.mrp, bbjq.mrp, gkdxy.mrp, jjfbol/attack.mrp, jjfbol/bigWorldMap.mrp, jjfbol/bmm1.mrp, jjfbol/bmm2.mrp, jjfbol/bmm3.mrp, jjfbol/bmm4.mrp, jjfbol/bmm5.mrp, jjfbol/bmm6.mrp, jjfbol/bmm7.mrp, jjfbol/bmm8.mrp, jjfbol/cannon1.mrp, jjfbol/chi.mrp, jjfbol/default.mrp, jjfbol/default2.mrp, jjfbol/default3.mrp, jjfbol/devil1.mrp, jjfbol/devil2.mrp, jjfbol/devil4.mrp, jjfbol/dong1.mrp, jjfbol/dongimage.mrp, jjfbol/downimage1.mrp`
- `7fc8d5412ac5d932` count=12: `caccharge.mrp, cacddz.mrp, cacddzani.mrp, caclobby.mrp, caclottery.mrp, cacpublic.mrp, dload.mrp, embfrd.mrp, gbrwshell.mrp, gui.mrp, hotchat.mrp, wapgame.mrp`
- `ff67eea7e6eed10e` count=12: `directpay.mrp, gamelist.mrp, gbrwcore.mrp, pmsg.mrp, reglogin.mrp, resmng.mrp, rollscr.mrp, roomlist.mrp, smsbase.mrp, smscharge.mrp, svrctrl.mrp, vdload.mrp`
- `08be35830b3ba895` count=6: `xjwq/res/mrp/mmochat_res1.mrp, xjwq/res/mrp/mmochat_res2.mrp, xjwq/res/mrp/mmochat_res3.mrp, xjwq/res/mrp/mmochat_res4.mrp, xjwq/res/mrp/mmochat_res5.mrp, xjwq/res/mrp/mmochat_res6.mrp`
- `c8d664aa7034d044` count=2 ← JJFB/WXJWQ 共同模板: `jjfb.mrp, wxjwq.mrp`
- `aa518953e32491eb` count=1: `cardcharge.mrp`
- `343197ddca55c015` count=1: `font.mrp`
- `5addb579b1e95ce4` count=1: `sanguo.mrp`

### 4.2 mrc_loader.ext hash 分组
- `d36151ee3c119717` count=2: `jjfb.mrp, wxjwq.mrp`

## 5. Shell 核心包细节
### 5.x `gbrwcore.mrp`
- 路径：`gbrwcore.mrp`；size=100292；entries=3；class=`gwy_shell_core`。
- ext members：`reg.ext|gbrwcore.ext`。
- reg primary guess：`gbrwcore.ext`；reg ext names：`gbrwcore.ext`。

### 5.x `gbrwshell.mrp`
- 路径：`gbrwshell.mrp`；size=33152；entries=9；class=`gwy_shell_core`。
- ext members：`reg.ext|gbrwshell.ext`。
- reg primary guess：`gbrwshell.ext`；reg ext names：`gbrwshell.ext`。

### 5.x `gamelist.mrp`
- 路径：`gamelist.mrp`；size=72620；entries=26；class=`gwy_shell_core`。
- ext members：`gamelist.ext|reg.ext`。
- reg primary guess：`gamelist.ext`；reg ext names：`gamelist.ext`。

### 5.x `vdload.mrp`
- 路径：`vdload.mrp`；size=11586；entries=3；class=`native_ext_package`。
- ext members：`reg.ext|vdload.ext`。
- reg primary guess：`vdload.ext`；reg ext names：`vdload.ext`。

### 5.x `roomlist.mrp`
- 路径：`roomlist.mrp`；size=13627；entries=3；class=`native_ext_package`。
- ext members：`reg.ext|roomlist.ext`。
- reg primary guess：`roomlist.ext`；reg ext names：`roomlist.ext`。


## 6. gamelist.mrp 专项：为什么 6Q 应先修 member_view/primary
- `gamelist.mrp` entries=26，ext members=`gamelist.ext|reg.ext`，reg primary guess=`gamelist.ext`。
- 文件层结论：包内 `reg.ext` 字符串尾部含 `gamelist.ext`，所以 runtime 报 `no reg.ext primary` 时优先怀疑 member_view/reg parser 对 gamelist 的生成路径没覆盖，而不是原始包缺失。
- 6Q 应强制生成 `overlay/mrp_member_view/shell_gamelist_cfunction.mrp`，并让 `gwy/gamelist.mrp` fileopen 命中 generated member view，而不是 canonical 原包。
- 只有看到 `gamelist.ext` 的 extChunk/ER_RW/R9 与 native init_ok，才继续追 cfg36/no_update/native runapp。

## 7. JJFB / WXJWQ 对照
### `jjfb.mrp`
- size=414602 entries=50 class=mrc_loader_game
- start_sha256=`c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05`
- mrc_loader_sha256=`d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938`
- ext_members=`mrc_loader.ext|bigworldmapmodule.ext|mainmenumodule.ext|mailmodule.ext|moduletest.ext|chatmodule.ext|gezimodule.ext|taobaomodule.ext|gameattackmodule.ext|monsterstatemodule.ext|itembagshopmodule.ext|viewmanmodule.ext|betmodule.ext|monstermodule.ext|leitaimodule.ext|reg.ext|lianmengmodule.ext|othermodule.ext|robotol.ext|shopmodule.ext|itemhechengmodule.ext`
### `wxjwq.mrp`
- size=299067 entries=21 class=mrc_loader_game
- start_sha256=`c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05`
- mrc_loader_sha256=`d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938`
- ext_members=`mrc_loader.ext|reg.ext|mmochat.ext`

结论：`jjfb.mrp` 与 `wxjwq.mrp` 的 `start.mr` 和 `mrc_loader.ext` 是相同 bootstrap，后续凡是 mrc_loader 层故障都应优先用 wxjwq 对照排除 JJFB 特异性。

## 8. cfg.bin 路径记录与 JJFB 上下文
- cfg.bin size=20728 bytes。
- JJFB 关键偏移：`{'ng_jjfb.gif': 10880, 'gwy/jjfb.mrp': 10943}`。
- `csv/cfg_bin_path_records.csv` 已列出 cfg.bin 内所有 `gwy/*.mrp` 路径的偏移和前后字段十六进制。
- `gwy/jjfb.mrp` path_off=10943, pre16=`0000000001e200000000020000000001`, post48=`6777792f6a6a66622e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffff...`

## 9. 关键字符串证据（摘录）
### gamelist.mrp : gamelist.ext
- `0x13CE8` `gwy/gifs/`
- `0x13D58` `http://www.51mrp.com/wap/mpt/showThread/tid/702742`
- `0x13D8C` `http://i.51mrp.com/commonprofile/userpassport.ftl`
- `0x13E00` `logo.bmp`
- `0x13E20` `gwy/`
- `0x13EAC` `gwy/`
- `0x13EC0` `gwy/cfg.bin.td`
- `0x13ED0` `gwy/cfg.bin`
- `0x13EF8` `gamelist.bmp`
- `0x13F0E` `new.bmp`
- `0x13F24` `exit.bmp`
- `0x13F3A` `dtiele.bmp`
- `0x13F50` `border.bmp`
- `0x13F66` `dload.bmp`
- `0x13F7C` `dload1.bmp`
- `0x13F92` `head.bmp`
- `0x13FA8` `dl_list.bmp`
- `0x13FBE` `dbhead.bmp`
- `0x13FD4` `dbbot.bmp`
- `0x13FEA` `dbboard.bmp`
- `0x14000` `dbtitle.bmp`
- `0x14016` `1.bmp`
- `0x1402C` `2.bmp`
- `0x14042` `3.bmp`
- `0x14058` `4.bmp`
- `0x1406E` `5.bmp`
- `0x14084` `6.bmp`
- `0x140B4` `gwy/font.mrp`
- `0x140C4` `gwy/gbrwcore.mrp`
- `0x140D8` `gwy/gbrwshell.mrp`
- `0x1410C` `napptype=%d_nurl=%s_gwyblink`
- `0x1412C` `napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink`
- `0x1417C` `cfunction.ext`
- `0x141B0` `gwyblink`
- `0x141BC` `napptype`
- `0x141D0` `nextid`
- `0x141D8` `ncode`
- `0x141F0` `nmrpname`
- `0x141FC` `gwy/%s.mrp`
- `0x14210` `%s.mrp`
### gbrwcore.mrp : gbrwcore.ext
- `0x222D4` `lib.paramCat`
- `0x222E4` `lib.runflashmrp`
- `0x222F4` `lib.clearlogininfo`
- `0x22308` `lib.savelogininfo`
- `0x2231C` `lib.saveuserinfo`
- `0x22330` `lib.getuserinfo`
- `0x22340` `lib.appfuncasync`
- `0x22354` `lib.appfuncsync`
- `0x22364` `lib.showMenu`
- `0x22374` `lib.unfreeze`
- `0x22384` `lib.freeze`
- `0x22390` `lib.netpayR`
- `0x2239C` `lib.netpayrm`
- `0x223AC` `lib.sendsms`
- `0x223B8` `lib.netpaycheck`
- `0x223C8` `lib.alert`
- `0x223D4` `lib.startGame`
- `0x223E4` `lib.exitbrw`
- `0x223F0` `lib.reportresult`
- `0x22404` `lib.netpay`
- `0x22410` `lib.postfiledata`
- `0x22424` `lib.writefile`
- `0x22434` `lib.fileExist`
- `0x22444` `lib.checkmrpver`
- `0x22454` `lib.isFileOnServerNewer`
- `0x2246C` `lib.getmrpver`
- `0x2247C` `lib.getFileVersion`
- `0x22490` `lib.getClientInfo`
- `0x224A4` `lib.download`
- `0x224B4` `lib.loadurl`
- `0x224C0` `lib.runapp`
- `0x22538` `C:/Download`
- `0x2254C` `:/Download`
- `0x22558` `C:/Download/`
- `0x22568` `:/Download/`
- `0x22620` `http://`
- `0x22760` `.gif`
- `0x22770` `.bmp`
- `0x2279C` `http://mrp.fm`
- `0x227C4` `http/1.1`
### gbrwshell.mrp : gbrwshell.ext
- `0x9BD0` `cfunction.ext`
- `0x9BE0` `logo.ext`
- `0x9C20` `http://help.proxy.51mrp.com/help`
- `0x9C44` `http://help.proxy.51mrp.com/feedback`
- `0x9C6C` `http://help.proxy.51mrp.com/recommend`
- `0xA260` `wait.bmp`
- `0xA276` `extred.bmp`
- `0xA28C` `dlfail.bmp`
- `0xA2A2` `dled.bmp`
- `0xA2B8` `paused.bmp`
- `0xA2CE` `expnded.bmp`
- `0xA2E4` `dling.bmp`
- `0xA2FA` `fmimg.bmp`
- `0xA310` `fmfolder.bmp`
- `0xA326` `fmcmmn.bmp`
- `0xA33C` `fmback.bmp`
- `0xA352` `listf.bmp`
- `0xA368` `progbar.bmp`
- `0xA3D0` `.gif`
- `0xA400` `.bmp`
- `0xA4BC` `.gif`
- `0xA56A` `.bmp`
- `0xA6A9` `.mrp`
- `0xAB84` `http://dmrp.wapproxy.sky-mobi.com/sd?type=2&ver=0&appid=480006&shortname=wmplay&extid=6&mrpname=wmplay.mrp&extname=wmplay.ext&path=/plugins&func=1&param=NULL`
- `0xAC24` `/plugins/wmplay.mrp`
- `0xAC4C` `gbrw/download`
- `0xAC60` `%s/DOWNLOADLIST%d`
- `0xAC74` `gbrw/download`
- `0xAC84` `%s/TMPDOWNLOADLIST%d`
### vdload.mrp : vdload.ext
- `0x2F28` `spd.skymobiapp.com:6009`
- `0x2F74` `/continueDownload`
- `0x2F88` `/simpleDownload`
- `0x2F9C` `spd.skymobiapp.com`
### jjfb.mrp : robotol.ext
- `0x3A59C` `.mrp`
- `0x3A5C8` `default.mrp`
- `0x3A5D4` `default2.mrp`
- `0x3A5E4` `default3.mrp`
- `0x3A5F4` `jiantou_x!12!10.bmp`
- `0x3A608` `jiantou_t!12!10.bmp`
- `0x3A61C` `jiantou!12!15.bmp`
- `0x3A630` `wy_jiao5!11!11.bmp`
- `0x3A644` `wy_xian3!15!5.bmp`
- `0x3A658` `wy_jiao32!10!10.bmp`
- `0x3A66C` `wy_jiao3!10!10.bmp`
- `0x3A680` `wy_xian2!15!5.bmp`
- `0x3A694` `wy_jiao22!10!10.bmp`
- `0x3A6A8` `wy_jiao2!10!10.bmp`
- `0x3A6BC` `wy_xian0!15!6.bmp`
- `0x3A6D0` `wy_jiao02!10!10.bmp`
- `0x3A6E4` `wy_jiao0!10!10.bmp`
- `0x3A6F8` `wy_xian1!15!7.bmp`
- `0x3A70C` `wy_jiao12!11!11.bmp`
- `0x3A720` `wy_jiao1!11!11.bmp`
- `0x3A73C` `face!120!12.bmp`
- `0x3A7A0` `boygirl!40!15.bmp`
- `0x3A7B4` `star!8!8.bmp`
- `0x3A7C4` `keypress!28!21.bmp`
- `0x3A7D8` `e!7!9.bmp`
- `0x3A7E4` `finger!14!12.bmp`
- `0x3A7F8` `vip!34!12.bmp`
- `0x3A808` `downArrow!14!9.bmp`
- `0x3A81C` `updown!28!8.bmp`
- `0x3A82C` `upArrow!14!9.bmp`
- `0x3A840` `money!5!10.bmp`
- `0x3A850` `lrarrow!16!14.bmp`
- `0x3A864` `chilunbar!23!23.bmp`
- `0x3A878` `icon1!100!25.bmp`
- `0x3A88C` `buttons!36!12.bmp`
- `0x3A8A0` `bighand!25!25.bmp`
- `0x3A8B4` `topright!12!4.bmp`
- `0x3A8C8` `topleft!15!5.bmp`
- `0x3A8EC` `textbar!120!30.bmp`
- `0x3A900` `bar!16!18.bmp`
### jjfb.mrp : mrc_loader.ext
- `0xD4` `cfunction.ext`
### wxjwq.mrp : mmochat.ext
- `0x4A670` `tcph`
- `0x4AB24` `gwy/xjwq/loginserver.sys`
- `0x4AB40` `src\mmochat_serverlist.c`
- `0x4AB5C` `gwy/xjwq/`
- `0x4ADBC` `gwy/xjwq/res/mrp/mmochat_mapBaseInfo.rs`
- `0x4AE6C` `gwy/xjwq/xjwq.res`
- `0x4B9F4` `mmochat.ext`
- `0x4BA00` `gwy/rollscr.mrp`
- `0x4C254` `src\mmochat_update.c`
- `0x4C274` `gwy/xjwq/res/`
- `0x4C28C` `gwy/xjwq/res/mrp/`
- `0x4C2B0` `gwy/xjwq/xjwq.res`
- `0x4C2C4` `%smmochat_res%d.mrp.rs`
- `0x4C2DC` `face1.bmp`
- `0x4C2F2` `face2.bmp`
- `0x4C308` `face3.bmp`
- `0x4C31E` `face4.bmp`
- `0x4C334` `face5.bmp`
- `0x4C34A` `hill1.bmp`
- `0x4C360` `hill2.bmp`
- `0x4C376` `name1.bmp`
- `0x4C38C` `name2.bmp`
- `0x4C3A2` `name3.bmp`
- `0x4C3B8` `load1.bmp`
- `0x4C3CE` `load2.bmp`
- `0x4C3E4` `load3.bmp`
- `0x4C3FA` `zhang.bmp`
- `0x4C780` `gwy/xjwq/sfc.bin`
- `0x4C7A0` `gwy/xjwq/`
- `0x4C7E0` `line1.bmp`
- `0x4C7F6` `line2.bmp`
- `0x4C80C` `loghead.bmp`
- `0x4C822` `logfra1.bmp`
- `0x4C838` `logfra2.bmp`
- `0x4C84E` `logfra3.bmp`
- `0x4C864` `logfra4.bmp`
- `0x4C87A` `logbutt.bmp`
- `0x4C890` `logbutt2.bmp`
- `0x4C8A6` `serbg1.bmp`
- `0x4C8BC` `serbg2.bmp`

## 10. EXT 字段模式扫描怎么用
`focused/*.field_clusters.csv` 中已把重点 EXT 的 Thumb `LDR/STR [R?,#imm]` 扫描出来。优先关注包含 `+0/+4/+8/+0xC/+0x10` 的 cluster；这些 cluster 往往对应结构初始化、P/mr_c_function_st 或 extChunk-like 对象操作。这个扫描不是反汇编真值，只是快速索引，需要用动态 PC 命中验证。

## 11. 对后续调试的直接建议
### Phase 6Q 最短路线
1. 修 `gamelist.mrp` member_view：原包文件层存在 `reg.ext -> gamelist.ext`，runtime 不能再报 no primary。
2. 对 `gamelist.ext` 启用和 gbrwcore 同等平台上下文：extChunk provider、ER_RW bind、R9_SWITCH_OK。
3. 审计 `dsm:cfunction.ext helper=0xA4178`：避免 helper 地址被错当 module entry；检查 LR_PROXY 参数流。
4. 清掉 `0x8CC00 / ENTRY_ARGUMENT` 后，再追 `gamelist cfg36 build -> no_update -> native runapp`。
5. 仍不要做 slot matrix，除非出现真实 `[JJFB_EXTCHUNK_SLOT_CALL]`。

## 12. 文件层不能直接拿到的东西
- `cfg.bin` 的完整结构字段名仍需结合运行日志/旧平台源码确认；本报告给出了路径偏移和 JJFB 上下文，但没有把每个二进制字段强行命名。
- EXT 是 ARM/Thumb native code；本报告的字段扫描是 pattern index，不是完整反汇编。真实函数语义仍需动态 PC、寄存器和 call route 验证。
- `P+0x0C`、ER_RW、R9 的恢复属于平台运行态，不可能仅从资源包完全证明；但文件层已证明 shell/gamelist/jjfb/wxjwq 的共同契约方向。
