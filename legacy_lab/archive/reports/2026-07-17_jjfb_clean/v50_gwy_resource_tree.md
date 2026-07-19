# v50 GWY 资源树静态审计

> 生成方式：`python scripts/v50_gwy_resource_audit.py`。本报告只陈述静态文件事实；运行时请求需由 v50 runner 日志补充。

## 1. 资源根

- Mythroad root：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320`
- GWY root：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy`
- 文件总数：**1551**
- 总字节数：**25086608 B**

## 2. 一级目录

| 目录 | 文件数 | 字节数 | 当前用途判断 |
|---|---:|---:|---|
| `ajss/` | 1 | 559305 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `assg/` | 23 | 5146442 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `cacggl/` | 0 | 0 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `caclobby/` | 2 | 6475 | 大厅资源。 |
| `caclottery/` | 2 | 14717 | 抽奖/活动资源。 |
| `gifs/` | 88 | 82581 | GWY 列表、图标和入口图片资源；包含机甲风暴入口图。 |
| `gkdxy/` | 474 | 1483338 | 其他 GWY 游戏资源目录。 |
| `jjfbol/` | 119 | 1029289 | 机甲风暴在线版外置资源包；地图、怪物、动作、配置等 companion MRP。 |
| `mhxx/` | 80 | 725553 | 其他 GWY 游戏资源目录。 |
| `sanguo/` | 1 | 3606 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `save/` | 0 | 0 | 平台/游戏存档与缓存目录；当前包为空，但必须保留可写目录。 |
| `sgmj/` | 1 | 581795 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `smd/` | 7 | 2374384 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `sound/` | 4 | 1611 | 平台或游戏 MIDI 声音资源。 |
| `spacetime/` | 1 | 478 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `ssjx/` | 6 | 666591 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `tlbb/` | 61 | 610331 | 其他 GWY 游戏资源目录。 |
| `wm1/` | 5 | 10668 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `xjwq/` | 15 | 2274578 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |
| `xyol/` | 459 | 1951191 | 其他 GWY 游戏资源目录。 |
| `yxlm/` | 154 | 162821 | GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。 |

## 3. GWY 根目录文件

- `ajss.mrp` — 377064 B
- `assg.mrp` — 244412 B
- `bbjq.mrp` — 342790 B
- `caccharge.mrp` — 113627 B
- `cacddz.mrp` — 89044 B
- `cacddzani.mrp` — 52301 B
- `caclobby.mrp` — 75312 B
- `caclottery.mrp` — 155848 B
- `cacpublic.mrp` — 49933 B
- `cardcharge.mrp` — 7241 B
- `cfg.bin` — 20728 B
- `directpay.mrp` — 24982 B
- `dload.mrp` — 10797 B
- `embfrd.mrp` — 8572 B
- `font.mrp` — 116098 B
- `gamelist.mrp` — 72620 B
- `gbrwcore.mrp` — 100292 B
- `gbrwshell.mrp` — 33152 B
- `gkdxy.mrp` — 299820 B
- `gui.mrp` — 32052 B
- `hotchat.mrp` — 30766 B
- `hxsg_cfg.bin` — 6898 B
- `jjfb.mrp` — 414602 B
- `pmsg.mrp` — 15242 B
- `qqgh_cfg.bin` — 6898 B
- `reglogin.mrp` — 30992 B
- `resmng.mrp` — 14610 B
- `rollscr.mrp` — 23519 B
- `roomlist.mrp` — 13627 B
- `sanguo.mrp` — 638474 B
- `sgmj.mrp` — 412941 B
- `smd.mrp` — 256866 B
- `smsbase.mrp` — 8915 B
- `smscharge.mrp` — 28521 B
- `spacetime.mrp` — 488893 B
- `ssjx.mrp` — 219731 B
- `svrctrl.mrp` — 11740 B
- `tlbb.mrp` — 307238 B
- `tyol.mrp` — 480559 B
- `vdload.mrp` — 11586 B
- `wapgame.mrp` — 7156 B
- `wm2.mrp` — 22626 B
- `wm2pet.mrp` — 34609 B
- `wxjwq.mrp` — 299067 B
- `xaqd.mrp` — 427604 B
- `xyol.mrp` — 324227 B
- `yxlm.mrp` — 350664 B
- `zsol.mrp` — 285598 B

## 4. 关键文件存在性

| 名称 | 主机顶层存在 | 说明 |
|---|---|---|
| `jjfb.mrp` | 是 | `game_files/mythroad/240x320/gwy/jjfb.mrp`，414602 B，SHA-256 `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` |
| `cfg.bin` | 是 | `game_files/mythroad/240x320/gwy/cfg.bin`，20728 B |
| `gbrwcore.mrp` | 是 | `game_files/mythroad/240x320/gwy/gbrwcore.mrp`，100292 B |
| `gbrwshell.mrp` | 是 | `game_files/mythroad/240x320/gwy/gbrwshell.mrp`，33152 B |
| `gamelist.mrp` | 是 | `game_files/mythroad/240x320/gwy/gamelist.mrp`，72620 B |
| `vdload.mrp` | 是 | `game_files/mythroad/240x320/gwy/vdload.mrp`，11586 B |
| `gamelist.ext` | 否 | 顶层实际是 `gamelist.mrp`（72620 B）；不要凭文档扩展名臆造文件。 |
| `gbrwcore.ext` | 否 | 顶层实际是 `gbrwcore.mrp`（100292 B）；不要凭文档扩展名臆造文件。 |
| `mrc_loader.ext` | 否（非顶层） | 位于 `jjfb.mrp` 内部，offset=3458, length=219 |
| `robotol.ext` | 否（非顶层） | 位于 `jjfb.mrp` 内部，offset=231594, length=161178 |

## 5. `jjfb.mrp` 内部启动项

| 内部项 | offset | length |
|---|---:|---:|
| `start.mr` | 1921 | 1514 |
| `mrc_loader.ext` | 3458 | 219 |
| `robotol.ext` | 231594 | 161178 |

共解析到 **50** 个内部条目。`mrc_loader.ext` 与 `robotol.ext` 是 MRP 内部项，不应复制为主机顶层文件。

## 6.1 `jjfbol/` 文件清单

- `jjfbol/attack.mrp` — 9105 B
- `jjfbol/attack.mrp.v` — 4 B
- `jjfbol/bigWorldMap.mrp` — 2644 B
- `jjfbol/bigWorldMap.mrp.v` — 4 B
- `jjfbol/bmm1.mrp` — 26443 B
- `jjfbol/bmm1.mrp.v` — 4 B
- `jjfbol/bmm2.mrp` — 25469 B
- `jjfbol/bmm2.mrp.v` — 4 B
- `jjfbol/bmm3.mrp` — 28335 B
- `jjfbol/bmm3.mrp.v` — 4 B
- `jjfbol/bmm4.mrp` — 25451 B
- `jjfbol/bmm4.mrp.v` — 4 B
- `jjfbol/bmm5.mrp` — 26837 B
- `jjfbol/bmm5.mrp.v` — 4 B
- `jjfbol/bmm6.mrp` — 26619 B
- `jjfbol/bmm6.mrp.v` — 4 B
- `jjfbol/bmm7.mrp` — 22246 B
- `jjfbol/bmm7.mrp.v` — 4 B
- `jjfbol/bmm8.mrp` — 25525 B
- `jjfbol/bmm8.mrp.v` — 4 B
- `jjfbol/cannon1.mrp` — 3506 B
- `jjfbol/cannon1.mrp.v` — 4 B
- `jjfbol/chi.mrp` — 3937 B
- `jjfbol/chi.mrp.v` — 4 B
- `jjfbol/default.mrp` — 17480 B
- `jjfbol/default.mrp.v` — 4 B
- `jjfbol/default2.mrp` — 19640 B
- `jjfbol/default2.mrp.v` — 4 B
- `jjfbol/default3.mrp` — 13415 B
- `jjfbol/default3.mrp.v` — 4 B
- `jjfbol/devil1.mrp` — 36608 B
- `jjfbol/devil1.mrp.v` — 4 B
- `jjfbol/devil2.mrp` — 3855 B
- `jjfbol/devil2.mrp.v` — 4 B
- `jjfbol/devil4.mrp` — 3502 B
- `jjfbol/devil4.mrp.v` — 4 B
- `jjfbol/dong1.mrp` — 16571 B
- `jjfbol/dong1.mrp.v` — 4 B
- `jjfbol/dongimage.mrp` — 2179 B
- `jjfbol/dongimage.mrp.v` — 4 B
- `jjfbol/downimage1.mrp` — 26668 B
- `jjfbol/downimage1.mrp.v` — 4 B
- `jjfbol/downimage2.mrp` — 23656 B
- `jjfbol/downimage2.mrp.v` — 4 B
- `jjfbol/downimage3.mrp` — 24117 B
- `jjfbol/downimage3.mrp.v` — 4 B
- `jjfbol/downVersion` — 4 B
- `jjfbol/gun1.mrp` — 10231 B
- `jjfbol/gun1.mrp.v` — 4 B
- `jjfbol/gun2.mrp` — 3982 B
- `jjfbol/gun2.mrp.v` — 4 B
- `jjfbol/gunimage.mrp` — 2111 B
- `jjfbol/gunimage.mrp.v` — 4 B
- `jjfbol/hint.mrp` — 2294 B
- `jjfbol/hint.mrp.v` — 4 B
- `jjfbol/itemType.mrp` — 16033 B
- `jjfbol/itemType.mrp.v` — 4 B
- `jjfbol/man1.mrp` — 10296 B
- `jjfbol/man1.mrp.v` — 4 B
- `jjfbol/man2.mrp` — 9750 B
- `jjfbol/man2.mrp.v` — 4 B
- `jjfbol/man3.mrp` — 10480 B
- `jjfbol/man3.mrp.v` — 4 B
- `jjfbol/mantecimg.mrp` — 2397 B
- `jjfbol/mantecimg.mrp.v` — 4 B
- `jjfbol/mapimage1.mrp` — 32460 B
- `jjfbol/mapimage1.mrp.v` — 4 B
- `jjfbol/mapimage2.mrp` — 33775 B
- `jjfbol/mapimage2.mrp.v` — 4 B
- `jjfbol/mapman.mrp` — 2674 B
- `jjfbol/mapman.mrp.v` — 4 B
- `jjfbol/mapValue.mrp` — 2568 B
- `jjfbol/mapValue.mrp.v` — 4 B
- `jjfbol/monster.mrp` — 10739 B
- `jjfbol/monster.mrp.v` — 4 B
- `jjfbol/monster1.mrp` — 39707 B
- `jjfbol/monster1.mrp.v` — 4 B
- `jjfbol/monster10.mrp` — 23574 B
- `jjfbol/monster10.mrp.v` — 4 B
- `jjfbol/monster2.mrp` — 37410 B
- `jjfbol/monster2.mrp.v` — 4 B
- `jjfbol/monster3.mrp` — 36437 B
- `jjfbol/monster3.mrp.v` — 4 B
- `jjfbol/monster4.mrp` — 39768 B
- `jjfbol/monster4.mrp.v` — 4 B
- `jjfbol/monster5.mrp` — 45739 B
- `jjfbol/monster5.mrp.v` — 4 B
- `jjfbol/monster6.mrp` — 33922 B
- `jjfbol/monster6.mrp.v` — 4 B
- `jjfbol/monster7.mrp` — 32448 B
- `jjfbol/monster7.mrp.v` — 4 B
- `jjfbol/monster8.mrp` — 37735 B
- `jjfbol/monster8.mrp.v` — 4 B
- `jjfbol/monster9.mrp` — 25315 B
- `jjfbol/monster9.mrp.v` — 4 B
- `jjfbol/mti.mrp` — 2385 B
- `jjfbol/mti.mrp.v` — 4 B
- `jjfbol/newzuojia.mrp` — 14792 B
- `jjfbol/newzuojia.mrp.v` — 4 B
- `jjfbol/state1.mrp` — 2170 B
- `jjfbol/state1.mrp.v` — 4 B
- `jjfbol/tecbuff.mrp` — 2509 B
- `jjfbol/tecbuff.mrp.v` — 4 B
- `jjfbol/vmimage.mrp` — 9828 B
- `jjfbol/vmimage.mrp.v` — 4 B
- `jjfbol/weapon1.mrp` — 17584 B
- `jjfbol/weapon1.mrp.v` — 4 B
- `jjfbol/weaponimage.mrp` — 2224 B
- `jjfbol/weaponimage.mrp.v` — 4 B
- `jjfbol/wing1.mrp` — 20357 B
- `jjfbol/wing1.mrp.v` — 4 B
- `jjfbol/wing2.mrp` — 3175 B
- `jjfbol/wing2.mrp.v` — 4 B
- `jjfbol/wingimage.mrp` — 2135 B
- `jjfbol/wingimage.mrp.v` — 4 B
- `jjfbol/xg1.mrp` — 18015 B
- `jjfbol/xg1.mrp.v` — 4 B
- `jjfbol/xg2.mrp` — 18222 B
- `jjfbol/xg2.mrp.v` — 4 B

## 6.2 `gifs/` 文件清单

- `gifs/dj_gg9.gif` — 751 B
- `gifs/dwol.gif` — 523 B
- `gifs/gg_cac_ddz.gif` — 2405 B
- `gifs/gg_mpsc1.gif` — 1653 B
- `gifs/gg_ng_ajol5.gif` — 2466 B
- `gifs/gg_ng_assg.gif` — 2963 B
- `gifs/gg_ng_cyqy2.gif` — 3576 B
- `gifs/gg_ng_dwol02.gif` — 2602 B
- `gifs/gg_ng_flsg.gif` — 2537 B
- `gifs/gg_ng_kdxy.gif` — 2483 B
- `gifs/gg_ng_sgmc3.gif` — 2483 B
- `gifs/gg_ng_sgzc3.gif` — 2596 B
- `gifs/gg_ng_ssjx01.gif` — 709 B
- `gifs/gg_ng_sxz.gif` — 2649 B
- `gifs/gg_ng_tlbb11.gif` — 2309 B
- `gifs/gg_ng_tyol2.gif` — 2595 B
- `gifs/gg_ng_xajh4.gif` — 2483 B
- `gifs/gg_ng_xyj1.gif` — 2391 B
- `gifs/gg_ng_yxlm.gif` — 2274 B
- `gifs/gg_ng_zsol03.gif` — 2452 B
- `gifs/gg_sns_czzx2.gif` — 2158 B
- `gifs/gg_sns_grd.gif` — 1913 B
- `gifs/gg_sns_mpkj.gif` — 2534 B
- `gifs/gg_sns_qpyz.gif` — 2405 B
- `gifs/gg_sns_rmyy.gif` — 2196 B
- `gifs/gg_sns_wyzq.gif` — 2147 B
- `gifs/gg_sns_xjwq.gif` — 2597 B
- `gifs/gg_xblt.gif` — 1557 B
- `gifs/gifdesc.info` — 460 B
- `gifs/gifinfo.idx1` — 30 B
- `gifs/gifinfo.idx12` — 14 B
- `gifs/gifinfo.idx13` — 17 B
- `gifs/gifinfo.idx16` — 11 B
- `gifs/gifinfo.idx18` — 19 B
- `gifs/gifinfo.idx20` — 48 B
- `gifs/gifinfo.idx22` — 16 B
- `gifs/gifinfo.idx23` — 17 B
- `gifs/gifinfo.idx25` — 15 B
- `gifs/gifinfo.idx26` — 18 B
- `gifs/gifinfo.idx28` — 15 B
- `gifs/gifinfo.idx31` — 13 B
- `gifs/gifinfo.idx34` — 15 B
- `gifs/gifinfo.idx36` — 15 B
- `gifs/gifinfo.idx39` — 15 B
- `gifs/gifinfo.idx40` — 18 B
- `gifs/gifinfo.idx41` — 15 B
- `gifs/gifinfo.idx42` — 18 B
- `gifs/gifinfo.idx44` — 18 B
- `gifs/gifinfo.idx5` — 19 B
- `gifs/gifinfo.idx50` — 18 B
- `gifs/gifinfo.idx54` — 15 B
- `gifs/gifinfo.idx55` — 18 B
- `gifs/gifinfo.idx57` — 18 B
- `gifs/gifinfo.idx60` — 18 B
- `gifs/gifinfo.idx61` — 17 B
- `gifs/gifinfo.idx67` — 18 B
- `gifs/gifinfo.idx69` — 35 B
- `gifs/gifinfo.idx70` — 14 B
- `gifs/gifinfo.idx71` — 18 B
- `gifs/gifinfo.idx76` — 14 B
- `gifs/gifinfo.idx83` — 19 B
- `gifs/gifinfo.idx88` — 15 B
- `gifs/gifinfo.idx89` — 14 B
- `gifs/gifinfo.idx9` — 17 B
- `gifs/gifinfo.idx91` — 11 B
- `gifs/gifinfo.idx93` — 17 B
- `gifs/gifinfo.idx94` — 25 B
- `gifs/gifinfo.idx95` — 17 B
- `gifs/gifinfo.idx98` — 11 B
- `gifs/gifinfo.idx99` — 14 B
- `gifs/mpsc.gif` — 1122 B
- `gifs/mpwygw2.gif` — 430 B
- `gifs/mssj.gif` — 1066 B
- `gifs/ng_assg.gif` — 1278 B
- `gifs/ng_bbjq1.gif` — 1881 B
- `gifs/ng_jjfb.gif` — 1195 B
- `gifs/ng_kdxy.gif` — 1276 B
- `gifs/ng_sxz1.gif` — 1275 B
- `gifs/ng_yxlm02.gif` — 826 B
- `gifs/sns_mpgw.gif` — 765 B
- `gifs/sns_mpjy.gif` — 1273 B
- `gifs/sns_mplt.gif` — 1261 B
- `gifs/sns_mpmh.gif` — 685 B
- `gifs/sns_mpzx.gif` — 720 B
- `gifs/wapxyj.gif` — 1078 B
- `gifs/wyzq_xxz.gif` — 580 B
- `gifs/xblt.gif` — 1093 B
- `gifs/yxol.gif` — 1181 B

## 7. 静态结论

1. `gwy/jjfb.mrp` 不是孤立文件；启动契约必须保留 `gwy/` 整棵目录。
2. `jjfbol/`、`gifs/`、`save/`、`sound/` 都应映射到同一个 GWY root。
3. `mrc_loader.ext`、`robotol.ext` 位于 `jjfb.mrp` 内部，顶层缺失不等于资源缺失。
4. 顶层实际文件是 `gbrwcore.mrp`、`gamelist.mrp` 等；文档里出现 `.ext` 时必须以真实包为准。
5. 动态结论必须以 `[JJFB_FILEOPEN]` / `[JJFB_FILEOPEN_MISS]` 为证据，不能再由 UI 是否显示反推。
