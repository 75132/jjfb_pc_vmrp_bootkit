# File-layer Dossier：GWY / JJFB / WXJWQ 原始文件资料

## 1. 核心包一览

| basename      |   size |   entry_count | class              | reg_primary_guess   |   start_len | has_mrc_loader_ext   |   asset_count | ext_members                                                                                                                                                                                                                                                                                                                                                           |
|:--------------|-------:|--------------:|:-------------------|:--------------------|------------:|:---------------------|--------------:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| gamelist.mrp  |  72620 |            26 | gwy_shell_core     | gamelist.ext        |        2490 | False                |            23 | gamelist.ext|reg.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwcore.mrp  | 100292 |             3 | gwy_shell_core     | gbrwcore.ext        |        2490 | False                |             0 | reg.ext|gbrwcore.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwshell.mrp |  33152 |             9 | gwy_shell_core     | gbrwshell.ext       |        2240 | False                |             6 | reg.ext|gbrwshell.ext                                                                                                                                                                                                                                                                                                                                                 |
| jjfb.mrp      | 414602 |            50 | mrc_loader_game    | robotol.ext         |        3787 | True                 |            54 | mrc_loader.ext|bigworldmapmodule.ext|mainmenumodule.ext|mailmodule.ext|moduletest.ext|chatmodule.ext|gezimodule.ext|taobaomodule.ext|gameattackmodule.ext|monsterstatemodule.ext|itembagshopmodule.ext|viewmanmodule.ext|betmodule.ext|monstermodule.ext|leitaimodule.ext|reg.ext|lianmengmodule.ext|othermodule.ext|robotol.ext|shopmodule.ext|itemhechengmodule.ext |
| roomlist.mrp  |  13627 |             3 | native_ext_package | roomlist.ext        |        2490 | False                |             0 | reg.ext|roomlist.ext                                                                                                                                                                                                                                                                                                                                                  |
| sanguo.mrp    | 638474 |           275 | native_ext_package | sanguo.ext          |      559903 | False                |           271 | reg.ext|sanguo.ext                                                                                                                                                                                                                                                                                                                                                    |
| spacetime.mrp | 488893 |           422 | native_ext_package | spacetime.ext       |        3435 | False                |           282 | reg.ext|spacetime.ext                                                                                                                                                                                                                                                                                                                                                 |
| tlbb.mrp      | 307238 |            18 | native_ext_package | dream.ext           |        3435 | False                |            11 | reg.ext|dream.ext                                                                                                                                                                                                                                                                                                                                                     |
| vdload.mrp    |  11586 |             3 | native_ext_package | vdload.ext          |        2490 | False                |             0 | reg.ext|vdload.ext                                                                                                                                                                                                                                                                                                                                                    |
| wxjwq.mrp     | 299067 |            21 | mrc_loader_game    | mmochat.ext         |        3787 | True                 |            17 | mrc_loader.ext|reg.ext|mmochat.ext                                                                                                                                                                                                                                                                                                                                    |

## 2. Shell key strings：gamelist.mrp

| member       |   off | s                                                                    |
|:-------------|------:|:---------------------------------------------------------------------|
| gamelist.ext | 81128 | gwy/gifs/                                                            |
| gamelist.ext | 81408 | logo.bmp                                                             |
| gamelist.ext | 81440 | gwy/                                                                 |
| gamelist.ext | 81580 | gwy/                                                                 |
| gamelist.ext | 81600 | gwy/cfg.bin.td                                                       |
| gamelist.ext | 81616 | gwy/cfg.bin                                                          |
| gamelist.ext | 81656 | gamelist.bmp                                                         |
| gamelist.ext | 81678 | new.bmp                                                              |
| gamelist.ext | 81700 | exit.bmp                                                             |
| gamelist.ext | 81722 | dtiele.bmp                                                           |
| gamelist.ext | 81744 | border.bmp                                                           |
| gamelist.ext | 81766 | dload.bmp                                                            |
| gamelist.ext | 81788 | dload1.bmp                                                           |
| gamelist.ext | 81810 | head.bmp                                                             |
| gamelist.ext | 81832 | dl_list.bmp                                                          |
| gamelist.ext | 81854 | dbhead.bmp                                                           |
| gamelist.ext | 81876 | dbbot.bmp                                                            |
| gamelist.ext | 81898 | dbboard.bmp                                                          |
| gamelist.ext | 81920 | dbtitle.bmp                                                          |
| gamelist.ext | 81942 | 1.bmp                                                                |
| gamelist.ext | 81964 | 2.bmp                                                                |
| gamelist.ext | 81986 | 3.bmp                                                                |
| gamelist.ext | 82008 | 4.bmp                                                                |
| gamelist.ext | 82030 | 5.bmp                                                                |
| gamelist.ext | 82052 | 6.bmp                                                                |
| gamelist.ext | 82100 | gwy/font.mrp                                                         |
| gamelist.ext | 82116 | gwy/gbrwcore.mrp                                                     |
| gamelist.ext | 82136 | gwy/gbrwshell.mrp                                                    |
| gamelist.ext | 82188 | napptype=%d_nurl=%s_gwyblink                                         |
| gamelist.ext | 82220 | napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink |
| gamelist.ext | 82300 | cfunction.ext                                                        |
| gamelist.ext | 82352 | gwyblink                                                             |
| gamelist.ext | 82364 | napptype                                                             |
| gamelist.ext | 82384 | nextid                                                               |
| gamelist.ext | 82392 | ncode                                                                |
| gamelist.ext | 82416 | nmrpname                                                             |
| gamelist.ext | 82428 | gwy/%s.mrp                                                           |
| gamelist.ext | 82448 | %s.mrp                                                               |
| gamelist.ext | 82528 | gwy.mrp                                                              |
| gamelist.ext | 82536 | reglogin.mrp                                                         |
| gamelist.ext | 82552 | resmng.mrp                                                           |
| gamelist.ext | 82564 | dload.mrp                                                            |
| gamelist.ext | 82576 | gamelist.mrp                                                         |
| gamelist.ext | 82592 | gui.mrp                                                              |
| gamelist.ext | 82624 | gwy/                                                                 |
| gamelist.ext | 82632 | gwy\wizard.mrp                                                       |
| gamelist.ext | 82648 | gwy\hotchat.mrp                                                      |
| gamelist.ext | 82684 | gwy/sound/                                                           |
| gamelist.ext | 90044 | Content-Type: application/x-www-form-urlencoded;charset=UTF-8        |
| gamelist.ext | 90136 | mrc_freesky.51mrp.com                                                |
| reg.ext      |   848 | gamelist.ext                                                         |

## 3. Shell key strings：gbrwcore.mrp

| member       |    off | s                                                             |
|:-------------|-------:|:--------------------------------------------------------------|
| reg.ext      |    664 | gbrwcore.ext                                                  |
| gbrwcore.ext | 140020 | lib.clearlogininfo                                            |
| gbrwcore.ext | 140040 | lib.savelogininfo                                             |
| gbrwcore.ext | 140244 | lib.startGame                                                 |
| gbrwcore.ext | 140272 | lib.reportresult                                              |
| gbrwcore.ext | 140372 | lib.isFileOnServerNewer                                       |
| gbrwcore.ext | 140452 | lib.download                                                  |
| gbrwcore.ext | 140480 | lib.runapp                                                    |
| gbrwcore.ext | 140600 | C:/Download                                                   |
| gbrwcore.ext | 140620 | :/Download                                                    |
| gbrwcore.ext | 140632 | C:/Download/                                                  |
| gbrwcore.ext | 140648 | :/Download/                                                   |
| gbrwcore.ext | 141168 | .bmp                                                          |
| gbrwcore.ext | 142048 | .bmp                                                          |
| gbrwcore.ext | 142388 | sky_download:                                                 |
| gbrwcore.ext | 142812 | cfunction.ext                                                 |
| gbrwcore.ext | 142828 | logo.ext                                                      |
| gbrwcore.ext | 143286 | .bmp                                                          |
| gbrwcore.ext | 143605 | .mrp                                                          |
| gbrwcore.ext | 145700 | download                                                      |
| gbrwcore.ext | 145720 | download/dwnlist.dat                                          |
| gbrwcore.ext | 146728 | Content-Type: application/x-www-form-urlencoded;charset=UTF-8 |
| gbrwcore.ext | 146820 | mrc_freesky.51mrp.com                                         |

## 4. Shell key strings：gbrwshell.mrp

| member        |   off | s                                                                                                                                                             |
|:--------------|------:|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| reg.ext       |   608 | gbrwshell.ext                                                                                                                                                 |
| gbrwshell.ext | 39888 | cfunction.ext                                                                                                                                                 |
| gbrwshell.ext | 39904 | logo.ext                                                                                                                                                      |
| gbrwshell.ext | 41568 | wait.bmp                                                                                                                                                      |
| gbrwshell.ext | 41590 | extred.bmp                                                                                                                                                    |
| gbrwshell.ext | 41612 | dlfail.bmp                                                                                                                                                    |
| gbrwshell.ext | 41634 | dled.bmp                                                                                                                                                      |
| gbrwshell.ext | 41656 | paused.bmp                                                                                                                                                    |
| gbrwshell.ext | 41678 | expnded.bmp                                                                                                                                                   |
| gbrwshell.ext | 41700 | dling.bmp                                                                                                                                                     |
| gbrwshell.ext | 41722 | fmimg.bmp                                                                                                                                                     |
| gbrwshell.ext | 41744 | fmfolder.bmp                                                                                                                                                  |
| gbrwshell.ext | 41766 | fmcmmn.bmp                                                                                                                                                    |
| gbrwshell.ext | 41788 | fmback.bmp                                                                                                                                                    |
| gbrwshell.ext | 41810 | listf.bmp                                                                                                                                                     |
| gbrwshell.ext | 41832 | progbar.bmp                                                                                                                                                   |
| gbrwshell.ext | 41984 | .bmp                                                                                                                                                          |
| gbrwshell.ext | 42346 | .bmp                                                                                                                                                          |
| gbrwshell.ext | 42665 | .mrp                                                                                                                                                          |
| gbrwshell.ext | 43908 | http://dmrp.wapproxy.sky-mobi.com/sd?type=2&ver=0&appid=480006&shortname=wmplay&extid=6&mrpname=wmplay.mrp&extname=wmplay.ext&path=/plugins&func=1&param=NULL |
| gbrwshell.ext | 44068 | /plugins/wmplay.mrp                                                                                                                                           |
| gbrwshell.ext | 44108 | gbrw/download                                                                                                                                                 |
| gbrwshell.ext | 44128 | %s/DOWNLOADLIST%d                                                                                                                                             |
| gbrwshell.ext | 44148 | gbrw/download                                                                                                                                                 |
| gbrwshell.ext | 44164 | %s/TMPDOWNLOADLIST%d                                                                                                                                          |

## 5. JJFB key strings / resources / modules

| member                |    off | s                            |
|:----------------------|-------:|:-----------------------------|
| mrc_loader.ext        |    212 | cfunction.ext                |
| bigworldmapmodule.ext |  16308 | mapcity!150!30.bmp           |
| bigworldmapmodule.ext |  16328 | mapload!23!15.bmp            |
| mainmenumodule.ext    |  26752 | serversel                    |
| mainmenumodule.ext    |  27144 | m8!160!120@bmm8.bmp          |
| mainmenumodule.ext    |  27164 | m7!160!120@bmm7.bmp          |
| mainmenumodule.ext    |  27184 | m6!160!120@bmm6.bmp          |
| mainmenumodule.ext    |  27204 | m5!160!120@bmm5.bmp          |
| mainmenumodule.ext    |  27224 | m4!160!120@bmm4.bmp          |
| mainmenumodule.ext    |  27244 | m3!160!120@bmm3.bmp          |
| mainmenumodule.ext    |  27264 | m2!160!120@bmm2.bmp          |
| mainmenumodule.ext    |  27284 | m1!160!120@bmm1.bmp          |
| mainmenumodule.ext    |  27304 | m4!120!160@mainmenu4.bmp     |
| mainmenumodule.ext    |  27332 | m3!120!160@mainmenu3.bmp     |
| mainmenumodule.ext    |  27360 | m2!120!160@mainmenu2.bmp     |
| mainmenumodule.ext    |  27388 | m1!120!160@mainmenu1.bmp     |
| mainmenumodule.ext    |  27424 | top!76!28.bmp                |
| mainmenumodule.ext    |  27676 | robot2!165!115.bmp           |
| mainmenumodule.ext    |  27696 | robot1!191!104.bmp           |
| mainmenumodule.ext    |  27716 | menutext!76!80.bmp           |
| mailmodule.ext        |  25744 | fujian!15!15.bmp             |
| mailmodule.ext        |  25764 | money!13!14.bmp              |
| moduletest.ext        |  13984 | @serverDo=true               |
| chatmodule.ext        |  13472 | face!120!12.bmp              |
| gameattackmodule.ext  |  45184 | wy_baoji!120!14.bmp          |
| gameattackmodule.ext  |  45204 | wy_shanbi!44!14.bmp          |
| gameattackmodule.ext  |  45636 | wy_jiaxue!110!12.bmp         |
| gameattackmodule.ext  |  45660 | wy_shanghai!100!12.bmp       |
| gameattackmodule.ext  |  45708 | effect!49!30.bmp             |
| gameattackmodule.ext  |  46276 | shandow!49!17@attack.bmp     |
| itembagshopmodule.ext |  42028 | bag_hand!17!16.bmp           |
| itembagshopmodule.ext |  42048 | money!13!14.bmp              |
| viewmanmodule.ext     |  18328 | bag_hand!17!16.bmp           |
| reg.ext               |   2104 | robotol.ext                  |
| reg.ext               |   2116 | moduletest.ext               |
| reg.ext               |   2132 | mainmenumodule.ext           |
| reg.ext               |   2152 | bigworldmapmodule.ext        |
| reg.ext               |   2176 | gameattackmodule.ext         |
| reg.ext               |   2200 | taobaomodule.ext             |
| reg.ext               |   2220 | gezimodule.ext               |
| reg.ext               |   2236 | mailmodule.ext               |
| reg.ext               |   2252 | chatmodule.ext               |
| reg.ext               |   2268 | monsterstatemodule.ext       |
| reg.ext               |   2292 | viewmanmodule.ext            |
| reg.ext               |   2312 | itembagshopmodule.ext        |
| reg.ext               |   2336 | leitaimodule.ext             |
| reg.ext               |   2356 | monstermodule.ext            |
| reg.ext               |   2376 | lianmengmodule.ext           |
| reg.ext               |   2396 | othermodule.ext              |
| reg.ext               |   2412 | itemhechengmodule.ext        |
| reg.ext               |   2436 | shopmodule.ext               |
| reg.ext               |   2452 | betmodule.ext                |
| robotol.ext           | 239004 | .mrp                         |
| robotol.ext           | 239048 | default.mrp                  |
| robotol.ext           | 239060 | default2.mrp                 |
| robotol.ext           | 239076 | default3.mrp                 |
| robotol.ext           | 239092 | jiantou_x!12!10.bmp          |
| robotol.ext           | 239112 | jiantou_t!12!10.bmp          |
| robotol.ext           | 239132 | jiantou!12!15.bmp            |
| robotol.ext           | 239152 | wy_jiao5!11!11.bmp           |
| robotol.ext           | 239172 | wy_xian3!15!5.bmp            |
| robotol.ext           | 239192 | wy_jiao32!10!10.bmp          |
| robotol.ext           | 239212 | wy_jiao3!10!10.bmp           |
| robotol.ext           | 239232 | wy_xian2!15!5.bmp            |
| robotol.ext           | 239252 | wy_jiao22!10!10.bmp          |
| robotol.ext           | 239272 | wy_jiao2!10!10.bmp           |
| robotol.ext           | 239292 | wy_xian0!15!6.bmp            |
| robotol.ext           | 239312 | wy_jiao02!10!10.bmp          |
| robotol.ext           | 239332 | wy_jiao0!10!10.bmp           |
| robotol.ext           | 239352 | wy_xian1!15!7.bmp            |
| robotol.ext           | 239372 | wy_jiao12!11!11.bmp          |
| robotol.ext           | 239392 | wy_jiao1!11!11.bmp           |
| robotol.ext           | 239420 | face!120!12.bmp              |
| robotol.ext           | 239520 | boygirl!40!15.bmp            |
| robotol.ext           | 239540 | star!8!8.bmp                 |
| robotol.ext           | 239556 | keypress!28!21.bmp           |
| robotol.ext           | 239576 | e!7!9.bmp                    |
| robotol.ext           | 239588 | finger!14!12.bmp             |
| robotol.ext           | 239608 | vip!34!12.bmp                |
| robotol.ext           | 239624 | downArrow!14!9.bmp           |
| robotol.ext           | 239644 | updown!28!8.bmp              |
| robotol.ext           | 239660 | upArrow!14!9.bmp             |
| robotol.ext           | 239680 | money!5!10.bmp               |
| robotol.ext           | 239696 | lrarrow!16!14.bmp            |
| robotol.ext           | 239716 | chilunbar!23!23.bmp          |
| robotol.ext           | 239736 | icon1!100!25.bmp             |
| robotol.ext           | 239756 | buttons!36!12.bmp            |
| robotol.ext           | 239776 | bighand!25!25.bmp            |
| robotol.ext           | 239796 | topright!12!4.bmp            |
| robotol.ext           | 239816 | topleft!15!5.bmp             |
| robotol.ext           | 239852 | textbar!120!30.bmp           |
| robotol.ext           | 239872 | bar!16!18.bmp                |
| robotol.ext           | 239888 | loadingbar!201!29.bmp        |
| robotol.ext           | 239912 | gunModel!47!28.bmp           |
| robotol.ext           | 239932 | wingModel!72!41.bmp          |
| robotol.ext           | 239952 | weaponModel!41!42.bmp        |
| robotol.ext           | 239976 | dongModel!28!48.bmp          |
| robotol.ext           | 240704 | dir!100!100@vmimage.bmp      |
| robotol.ext           | 240728 | vmleft!57!36@vmimage.bmp     |
| robotol.ext           | 240756 | vmright!57!36@vmimage.bmp    |
| robotol.ext           | 240784 | taskbutton!57!36@vmimage.bmp |
| robotol.ext           | 240904 | gwy/jjfbol/                  |
| robotol.ext           | 240916 | buttons!36!12.bmp            |
| robotol.ext           | 240936 | keypress!28!21.bmp           |
| robotol.ext           | 242888 | .bmp                         |
| robotol.ext           | 242928 | slogo!157!58.bmp             |
| robotol.ext           | 242948 | textbar!120!30.bmp           |
| robotol.ext           | 242968 | bar!16!18.bmp                |
| robotol.ext           | 242984 | loadingbar!201!29.bmp        |
| robotol.ext           | 243540 | zuobiao!60!8.bmp             |
| robotol.ext           | 243560 | dirarrow!13!12.bmp           |
| robotol.ext           | 243592 | monstersheji!48!16.bmp       |
| robotol.ext           | 243616 | monsterquanneng!48!16.bmp    |
| robotol.ext           | 243644 | monstergedou!48!16.bmp       |
| robotol.ext           | 243696 | fasticon!112!16.bmp          |
| robotol.ext           | 243716 | cheng!8!8.bmp                |
| robotol.ext           | 243732 | smallicon!140!20.bmp         |
| robotol.ext           | 243756 | text2!36!12.bmp              |
| robotol.ext           | 243772 | upArrow!14!9.bmp             |
| robotol.ext           | 243792 | downArrow!14!9.bmp           |
| robotol.ext           | 243812 | exp!14!9.bmp                 |
| robotol.ext           | 243828 | frame!16!16.bmp              |
| robotol.ext           | 243844 | fuhao!40!16.bmp              |
| robotol.ext           | 243860 | lchars!96!9.bmp              |
| robotol.ext           | 243876 | shine!27!9.bmp               |
| robotol.ext           | 245720 | chilun!123!35.bmp            |
| robotol.ext           | 245912 | explight!102!15.bmp          |
| robotol.ext           | 245932 | moneylight!102!15.bmp        |
| robotol.ext           | 245956 | powerlight!138!29.bmp        |
| robotol.ext           | 245980 | teclight!102!15.bmp          |
| robotol.ext           | 246000 | defendlight!132!23.bmp       |
| robotol.ext           | 246024 | speedlight!102!15.bmp        |
| robotol.ext           | 246048 | lucklight!102!15.bmp         |
| robotol.ext           | 247436 | jt!30!16.bmp                 |
| robotol.ext           | 249724 | face!120!12.bmp              |

## 6. WXJWQ key strings / resources / modules

| member         |    off | s                                       |
|:---------------|-------:|:----------------------------------------|
| mrc_loader.ext |    212 | cfunction.ext                           |
| reg.ext        |    688 | mmochat.ext                             |
| mmochat.ext    | 305956 | gwy/xjwq/loginserver.sys                |
| mmochat.ext    | 305984 | src\mmochat_serverlist.c                |
| mmochat.ext    | 306012 | gwy/xjwq/                               |
| mmochat.ext    | 306620 | gwy/xjwq/res/mrp/mmochat_mapBaseInfo.rs |
| mmochat.ext    | 306796 | gwy/xjwq/xjwq.res                       |
| mmochat.ext    | 309748 | mmochat.ext                             |
| mmochat.ext    | 309760 | gwy/rollscr.mrp                         |
| mmochat.ext    | 311892 | src\mmochat_update.c                    |
| mmochat.ext    | 311924 | gwy/xjwq/res/                           |
| mmochat.ext    | 311948 | gwy/xjwq/res/mrp/                       |
| mmochat.ext    | 311984 | gwy/xjwq/xjwq.res                       |
| mmochat.ext    | 312004 | %smmochat_res%d.mrp.rs                  |
| mmochat.ext    | 312028 | face1.bmp                               |
| mmochat.ext    | 312050 | face2.bmp                               |
| mmochat.ext    | 312072 | face3.bmp                               |
| mmochat.ext    | 312094 | face4.bmp                               |
| mmochat.ext    | 312116 | face5.bmp                               |
| mmochat.ext    | 312138 | hill1.bmp                               |
| mmochat.ext    | 312160 | hill2.bmp                               |
| mmochat.ext    | 312182 | name1.bmp                               |
| mmochat.ext    | 312204 | name2.bmp                               |
| mmochat.ext    | 312226 | name3.bmp                               |
| mmochat.ext    | 312248 | load1.bmp                               |
| mmochat.ext    | 312270 | load2.bmp                               |
| mmochat.ext    | 312292 | load3.bmp                               |
| mmochat.ext    | 312314 | zhang.bmp                               |
| mmochat.ext    | 313216 | gwy/xjwq/sfc.bin                        |
| mmochat.ext    | 313248 | gwy/xjwq/                               |
| mmochat.ext    | 313312 | line1.bmp                               |
| mmochat.ext    | 313334 | line2.bmp                               |
| mmochat.ext    | 313356 | loghead.bmp                             |
| mmochat.ext    | 313378 | logfra1.bmp                             |
| mmochat.ext    | 313400 | logfra2.bmp                             |
| mmochat.ext    | 313422 | logfra3.bmp                             |
| mmochat.ext    | 313444 | logfra4.bmp                             |
| mmochat.ext    | 313466 | logbutt.bmp                             |
| mmochat.ext    | 313488 | logbutt2.bmp                            |
| mmochat.ext    | 313510 | serbg1.bmp                              |
| mmochat.ext    | 313532 | serbg2.bmp                              |
| mmochat.ext    | 313554 | logser.bmp                              |
| mmochat.ext    | 313576 | light.bmp                               |
| mmochat.ext    | 313598 | loguser.bmp                             |
| mmochat.ext    | 313620 | light2.bmp                              |
| mmochat.ext    | 313642 | rolebg1.bmp                             |
| mmochat.ext    | 313664 | rolebg2.bmp                             |
| mmochat.ext    | 313686 | rolebg3.bmp                             |
| mmochat.ext    | 313708 | rolebg4.bmp                             |
| mmochat.ext    | 313730 | pkbg.bmp                                |
| mmochat.ext    | 313752 | boy1_1.bmp                              |
| mmochat.ext    | 313774 | boy1_2.bmp                              |
| mmochat.ext    | 313796 | boy1_3.bmp                              |
| mmochat.ext    | 313818 | girl1_1.bmp                             |
| mmochat.ext    | 313840 | girl1_2.bmp                             |
| mmochat.ext    | 313862 | girl1_3.bmp                             |
| mmochat.ext    | 313884 | boy2_1.bmp                              |
| mmochat.ext    | 313906 | boy2_2.bmp                              |
| mmochat.ext    | 313928 | boy2_3.bmp                              |
| mmochat.ext    | 313950 | girl2_1.bmp                             |
| mmochat.ext    | 313972 | girl2_2.bmp                             |
| mmochat.ext    | 313994 | girl2_3.bmp                             |
| mmochat.ext    | 314016 | boy3_1.bmp                              |
| mmochat.ext    | 314038 | boy3_2.bmp                              |
| mmochat.ext    | 314060 | boy3_3.bmp                              |
| mmochat.ext    | 314082 | girl3_1.bmp                             |
| mmochat.ext    | 314104 | girl3_2.bmp                             |
| mmochat.ext    | 314126 | girl3_3.bmp                             |
| mmochat.ext    | 314148 | frame.bmp                               |
| mmochat.ext    | 314170 | list.bmp                                |
| mmochat.ext    | 314192 | arrow.bmp                               |
| mmochat.ext    | 314214 | arrow2.bmp                              |
| mmochat.ext    | 314236 | arrow3.bmp                              |
| mmochat.ext    | 314258 | m1.bmp                                  |
| mmochat.ext    | 314280 | m1_s.bmp                                |
| mmochat.ext    | 314302 | m2.bmp                                  |
| mmochat.ext    | 314324 | m2_s.bmp                                |
| mmochat.ext    | 314346 | m3.bmp                                  |
| mmochat.ext    | 314368 | m4.bmp                                  |
| mmochat.ext    | 314390 | m4_s.bmp                                |
| mmochat.ext    | 314412 | 1.bmp                                   |
| mmochat.ext    | 314434 | 2.bmp                                   |
| mmochat.ext    | 314456 | 3.bmp                                   |
| mmochat.ext    | 314478 | 4.bmp                                   |
| mmochat.ext    | 314500 | 5.bmp                                   |
| mmochat.ext    | 314522 | 6.bmp                                   |
| mmochat.ext    | 314544 | 7.bmp                                   |
| mmochat.ext    | 314566 | 8.bmp                                   |
| mmochat.ext    | 314588 | 9.bmp                                   |
| mmochat.ext    | 314610 | 10.bmp                                  |
| mmochat.ext    | 314632 | 11.bmp                                  |
| mmochat.ext    | 314654 | 13.bmp                                  |
| mmochat.ext    | 314676 | 14.bmp                                  |
| mmochat.ext    | 314698 | 15.bmp                                  |
| mmochat.ext    | 314720 | 16.bmp                                  |
| mmochat.ext    | 314742 | 17.bmp                                  |
| mmochat.ext    | 314764 | 18.bmp                                  |
| mmochat.ext    | 314786 | 19.bmp                                  |
| mmochat.ext    | 314808 | 20.bmp                                  |
| mmochat.ext    | 314830 | 21.bmp                                  |
| mmochat.ext    | 314852 | 22.bmp                                  |
| mmochat.ext    | 314874 | 30.bmp                                  |
| mmochat.ext    | 314896 | 31.bmp                                  |
| mmochat.ext    | 314918 | 32.bmp                                  |
| mmochat.ext    | 314940 | 33.bmp                                  |
| mmochat.ext    | 314962 | 34.bmp                                  |
| mmochat.ext    | 314984 | 35.bmp                                  |
| mmochat.ext    | 315006 | 37.bmp                                  |
| mmochat.ext    | 315028 | 39.bmp                                  |
| mmochat.ext    | 315050 | 40.bmp                                  |
| mmochat.ext    | 315072 | 42.bmp                                  |
| mmochat.ext    | 315094 | 43.bmp                                  |
| mmochat.ext    | 315116 | 45.bmp                                  |
| mmochat.ext    | 315138 | 46.bmp                                  |
| mmochat.ext    | 315160 | 48.bmp                                  |
| mmochat.ext    | 315182 | 49.bmp                                  |
| mmochat.ext    | 315204 | entry.bmp                               |
| mmochat.ext    | 315226 | entry2.bmp                              |
| mmochat.ext    | 315248 | 1_s.bmp                                 |
| mmochat.ext    | 315270 | 11_s.bmp                                |
| mmochat.ext    | 315292 | 13_s.bmp                                |
| mmochat.ext    | 315314 | 16_s.bmp                                |
| mmochat.ext    | 315336 | 49_s.bmp                                |
| mmochat.ext    | 315358 | entry_s.bmp                             |
| mmochat.ext    | 315380 | roletag.bmp                             |
| mmochat.ext    | 315402 | npctag.bmp                              |
| mmochat.ext    | 315424 | guai1.bmp                               |
| mmochat.ext    | 315446 | guai2.bmp                               |
| mmochat.ext    | 315468 | guai3.bmp                               |
| mmochat.ext    | 315490 | guai4.bmp                               |
| mmochat.ext    | 315512 | guai5.bmp                               |
| mmochat.ext    | 315534 | guai5_2.bmp                             |
| mmochat.ext    | 315556 | guai6.bmp                               |
| mmochat.ext    | 315578 | guai7.bmp                               |
| mmochat.ext    | 315600 | guai8.bmp                               |
| mmochat.ext    | 315622 | guai9.bmp                               |
| mmochat.ext    | 315644 | guai10.bmp                              |
| mmochat.ext    | 315666 | guai11.bmp                              |
| mmochat.ext    | 315688 | guai12.bmp                              |
| mmochat.ext    | 315710 | guai13.bmp                              |

## 7. JJFB member manifest

| member                 |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:-----------------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr               |          1514 |            3787 | True         | .mr   | c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05 |
| betmodule.ext          |          8327 |           14512 | True         | .ext  | 2bd3cee91fb99173522b6c83f33cdd1b885fb349874bfa0f8a4ce7b3e2bcb60e |
| bigworldmapmodule.ext  |          8741 |           17360 | True         | .ext  | baf01ab991598919473c35dc2e96c4574b10d62bf3c166c976a0437eda0ea3de |
| chatmodule.ext         |          8857 |           15228 | True         | .ext  | b1f7f8bc1846f164e7b7273931806118505086e20aedb29aab18714cbe455401 |
| gameattackmodule.ext   |         26089 |           47368 | True         | .ext  | 6f3afe7462a83168142f205a83da9fb22057c930c10feef8a652953d29f43b18 |
| gezimodule.ext         |          4261 |            7356 | True         | .ext  | 59d39d323fe634f0aeb65076672fe501a557c473a86f02f3ad7aa69f2525d490 |
| itembagshopmodule.ext  |         20439 |           43420 | True         | .ext  | de9a51caaf5fa084fae2bbf45579c3a3cbcdf5fad3f0032b59fa8fec8dd06887 |
| itemhechengmodule.ext  |         12957 |           25136 | True         | .ext  | fa4ffb041f016c82ac21745ea35e1e00bb93a218e0ca625f85b2b5c7e9504a06 |
| leitaimodule.ext       |         11387 |           21248 | True         | .ext  | b0a9ac472cc958acf06ecb6a55a6a6dc59942d2943a45d5918ee1e6f1c9c2885 |
| lianmengmodule.ext     |         17529 |           36216 | True         | .ext  | 3129f23b7b1038ab09a8017805ec662fbab9ae05fa65bcc0ccb8e2efa6ead93b |
| mailmodule.ext         |         14778 |           27692 | True         | .ext  | faad4e9f796c26658e8d7a3a97b5f38f3161b202e264499bb4dec3bb9cb1b640 |
| mainmenumodule.ext     |         15300 |           28712 | True         | .ext  | ecad68139c8f94b379a8c88c7facf42da5b6778ea50cd5e8a99718a3b769b4cd |
| moduletest.ext         |          7200 |           15152 | True         | .ext  | 120fdea40196e29b9cbfb355ba460ac568c3d26e3f008da6ad7b838975bc7e92 |
| monstermodule.ext      |          8870 |           19256 | True         | .ext  | 071e0a52f716500cac606c35d673c9fb6caead2e535f2cf2feb5aeb0415891ab |
| monsterstatemodule.ext |         10663 |           18712 | True         | .ext  | 39e73ba119de641eee0ea2b738b26ef297d784ecad4137ceba9784403901e5fb |
| mrc_loader.ext         |           219 |             232 | True         | .ext  | d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938 |
| othermodule.ext        |         24412 |           46708 | True         | .ext  | 060159526b5a84d11e7eaa9cb2b9c030d40319dd0b812bd0edacc0e280f15d70 |
| reg.ext                |          1381 |            2472 | True         | .ext  | 6aa5e6be29634f0ecddd27a142ff9a55f922a9e51f06b0ef2bf029995c02fb9d |
| robotol.ext            |        161178 |          253420 | True         | .ext  | 55f66f1cca810ae7febe31af7e2dba8d00e6296a31aa7613f4aaf9ce5bb2fe82 |
| shopmodule.ext         |          8820 |           14836 | True         | .ext  | 96736090d5f8b987df218dfbfe0e8f00f35c13f440b281ad3f468fb979409b9c |
| taobaomodule.ext       |          9352 |           16504 | True         | .ext  | 175a7268ae625ed9473b980ff59369159e6f1147fbb21df19fba4ac41c0a705b |
| viewmanmodule.ext      |         10976 |           19436 | True         | .ext  | 9cc25245d323d87f3f9de9a3a091e3eb127ea940030ce7c1e72a8e91bed6897c |
| bar!16!18.bmp          |            35 |             576 | True         | .bmp  | 3dfc15b75d265a3593991b6d076e41b678d341c681721628baac21cd081886eb |
| dirarrow!13!12.bmp     |            95 |             312 | True         | .bmp  | b74167de275cc489e573714b3c5a7af4d0df86a31e31cf6c353fde4accb0f866 |
| jiantou!12!15.bmp      |           108 |             360 | True         | .bmp  | 26cd05339d7819be0b6cff6781cf666f5fd614a55fe943594d606d021bf6f7c2 |
| jiantou_t!12!10.bmp    |            47 |             240 | True         | .bmp  | b5be03d78f1fc1c7b688b3e2c76e6c2239c1c2b2e533e72f207c380ea5981c94 |
| jiantou_x!12!10.bmp    |            45 |             240 | True         | .bmp  | 2983645fa9322a3339df218d9ffe615aac39c764f5caf6ab60889139f026b609 |
| jiao41!12!12.bmp       |            90 |             288 | True         | .bmp  | 77d895f0cc9fa2a9ce2f46c90269ca7e5dc7088280102b747fe850d8c291ae78 |
| jiao42!11!6.bmp        |            48 |             132 | True         | .bmp  | a252997e7ea6abb542f300257d4459eed5217c8616692da92afa473762396013 |
| jt!30!16.bmp           |           148 |             960 | True         | .bmp  | c65d7cdd7edb99447ff33268a7e56658bbddf7b502c0a08877a1c2bc897dc601 |
| listicon.bmp           |           184 |             800 | True         | .bmp  | 2828abd633027d0d3a890438149f574eb1b626262689f92027ae3b21e24c75f2 |
| loadingbar!201!29.bmp  |          1385 |           11658 | True         | .bmp  | 6ab17ba601918ab21e1ec7d802ed70027586f2ce231e5b11732e0f97a5d6435d |
| slogo!157!58.bmp       |         11897 |           18212 | True         | .bmp  | 919d94d541ad0863fd1e7b630475401cde0097801ccdc8a516ef48cb84773fa9 |
| target!65!25.bmp       |           351 |            3250 | True         | .bmp  | 636bacb7634ff7eda8c69ae9816decc99f62a9356c1986cdbe97b887255cba7a |
| textbar!120!30.bmp     |          1544 |            7200 | True         | .bmp  | 1ec9f3d46f79243d4c961afc2b61a241b213a22ef35f43999fd6f2a1adb13bbc |
| top!76!28.bmp          |          1321 |            4256 | True         | .bmp  | b2dbe87f1b037af5cb892bac0c0469f2d7d5415f9df7df4c1d2bb9f5f8d9da0a |
| wy_jiao0!10!10.bmp     |            89 |             200 | True         | .bmp  | de492a542e5aec02cee34d3508c0c728c09d5404ed24007211f44d74f90a0f64 |
| wy_jiao02!10!10.bmp    |            91 |             200 | True         | .bmp  | 57298e740114d972d8c14c4c9b9075ec5119be13acc3888512b7cf658f6e2786 |
| wy_jiao1!11!11.bmp     |            90 |             242 | True         | .bmp  | edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13 |
| wy_jiao12!11!11.bmp    |            51 |             242 | True         | .bmp  | a998e920d576ab639cdffad5b47f8f088431c8fa12f44747b68ebbb45f78b6d8 |
| wy_jiao2!10!10.bmp     |            67 |             200 | True         | .bmp  | e44746d176272d4165daa2e5733c7ea188c82762c8bba8fe8f65618d5613415c |
| wy_jiao22!10!10.bmp    |            48 |             200 | True         | .bmp  | 455e422004ed8dbfa1e822e656034aaa60d5c7fd77dd8409483f832680b66881 |
| wy_jiao3!10!10.bmp     |            67 |             200 | True         | .bmp  | 6c75abc1bb592f55c075d4681769205d25f65d11744b95155e967d3f3bd8878c |
| wy_jiao32!10!10.bmp    |            48 |             200 | True         | .bmp  | d0fa471ca30cf10edf1c18b0af25868b411870380e06d90499c9151532580361 |
| wy_jiao5!11!11.bmp     |            73 |             242 | True         | .bmp  | 8c32b77a50ba7638952ccdbfbd160b2c9ba7f618157222b1c563151ef52bcdc6 |
| wy_xian0!15!6.bmp      |            42 |             180 | True         | .bmp  | e581348ffc815e467b5314bdfc9bb887dc3ae02ded019975403af80037572899 |
| wy_xian1!15!7.bmp      |            36 |             210 | True         | .bmp  | 84d53fabd01318e635c5f87138f21d2eb3271d280f56ca9c0dd8b01f1554968d |
| wy_xian2!15!5.bmp      |            35 |             150 | True         | .bmp  | d38fb926de56c4243ea0145f5454b52ccfd19d683ebe4cb758743efabc4a5485 |
| wy_xian3!15!5.bmp      |            36 |             150 | True         | .bmp  | 872cd8cab86510eb19b578fec2ffa07af1792940c95835644cb59618bb73ce92 |
| target.ani             |           113 |             125 | True         | .ani  | b35ad4317927a0c190731b4fd15361d7c644653b9fe642852f51c3912702d83b |

## 8. WXJWQ member manifest

| member         |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:---------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr       |          1514 |            3787 | True         | .mr   | c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05 |
| mmochat.ext    |        206148 |          320292 | True         | .ext  | b7cf57219e8eedaba3c71129df2ed4b03b9918d9038d977191fa14f78199f0fd |
| mrc_loader.ext |           219 |             232 | True         | .ext  | d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938 |
| reg.ext        |           658 |             704 | True         | .ext  | fe21e8b00e67a42779aad07a0a5adc264395b88065888644d2a0351e85cc7204 |
| sfcidx.bin     |          1978 |            2020 | True         | .bin  | e19edf8805d7982bad464ce1f0c0541596d974753c992e55e96ef77f8fb28b9c |
| face1.bmp      |          4411 |           30720 | True         | .bmp  | f169a959f47843c83c964071b4983ce2760f078318c35fbc0f21ad9587d80731 |
| face2.bmp      |          5858 |           30720 | True         | .bmp  | 46454b24cfd087b92f2b4ebebc19f3a8c9ee7d2d5ccd192910d721bc6fe8ac33 |
| face3.bmp      |         10582 |           30720 | True         | .bmp  | dbfc11c586dcfb6bb83a42aae10d94c202214038918d4458607fdf749d75015e |
| face4.bmp      |         11632 |           30720 | True         | .bmp  | 4abc18479faebb44ac8114ba98c67d956df7bc0098f3653f120ff95c108e900e |
| face5.bmp      |          9314 |           30720 | True         | .bmp  | ebb2f58d620178bde8c3ef9dee8331af8c16d43ccff0a4c3ca5bf388c928721e |
| hill1.bmp      |         16861 |           35960 | True         | .bmp  | 812fdcf98e717e6891b094e5c347a8f07d3f87f04ce36eeba5ec2c6923d591fb |
| hill2.bmp      |         17987 |           42390 | True         | .bmp  | 58f04c1d9cc2ce19c3a00e07f40a515572cad87194eb608c5c06ea6e3b5bfca2 |
| load1.bmp      |           465 |            6154 | True         | .bmp  | c695a68743794db03540f809170ad13947e34572f4b305984cffb2153aa09a3e |
| load2.bmp      |           321 |            3006 | True         | .bmp  | 77620a4db7a9ee7040e6aabefebb24346a338393a9060bce6ca2abd0b612dd3c |
| load3.bmp      |           112 |             364 | True         | .bmp  | 71d989c1a2f248e0ea7a702e6b1c5182bc97f19ce2b54502e6659ecb5b58be22 |
| name1.bmp      |          3410 |           23940 | True         | .bmp  | 4e9df47fcedaf4fdbb00f529f972cddbe872d4a1948b0910677343c7ed50ef97 |
| name2.bmp      |           159 |             224 | True         | .bmp  | 98ca09c1f8f7385efa59e796c7867ae5602b4278861f8b299a7f869bfac85bed |
| name3.bmp      |           161 |             696 | True         | .bmp  | 6bee9896666d73c57e2e17665ee8bfdfe3e0eccc4f28fd2b890ec9304278cefc |
| zhang.bmp      |           691 |             684 | True         | .bmp  | 20c8026cb56ef9127699209745a35f016c6587d012b37b5579a795b3c1fc7ec6 |
| text.res       |          4359 |            8454 | True         | .res  | 6fb1df673795d20d47160949bd84fd8ad2661073f117dfefc7e9e38c2868d502 |
| world.path     |          1053 |            1694 | True         | .path | 597456b8468e98cb640319f83581dc309a3b6879ef28a76a5f7e86cda6c06f17 |

## 9. Shell package member manifests

### gbrwcore.mrp

| member       |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:-------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr     |          1018 |            2490 | True         | .mr   | ff67eea7e6eed10e3871bd465953e7137001e2495b1eb13309c56db8659bfc7c |
| gbrwcore.ext |         98264 |          147196 | True         | .ext  | bad9ffb88d41d26a7db81ae41a449e2476ea615947ab06819a6ac13a55c26270 |
| reg.ext      |           638 |             684 | True         | .ext  | 8cd1a97b5cb27f9cd6ab6e67d496b9acf3b0cd48386efe17c345b36341dd8d0c |

### gbrwshell.mrp

| member        |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:--------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr      |           917 |            2240 | True         | .mr   | 7fc8d5412ac5d932196ebd2b1f22b042965442057f604371f3025d072e94c5fb |
| gbrwshell.ext |         29946 |           45216 | True         | .ext  | 5ebecf0d9fc8e77d2c8efd5b3c173702e7a1ee52d8911b518e3a885ce512e799 |
| reg.ext       |           598 |             628 | True         | .ext  | 1de1bb39d67acca823b02f379347fa440f19c4cf512f0c11018dda0ba9deeb29 |
| fmback.bmp    |           157 |             448 | True         | .bmp  | 976e8d22eb13368092523f97812840be71d112fd37870ed5158a77d264c87517 |
| fmcmmn.bmp    |           253 |             448 | True         | .bmp  | 91a55b79806879d1f4359a5ec84a6377ae237f50e11bfe5458b5f40f469d862e |
| fmfolder.bmp  |           156 |             448 | True         | .bmp  | c8af9998b2af6fe36a4bdb76e9c929a59f46c03d55ba65f82d4afcbced08d68f |
| fmimg.bmp     |           302 |             448 | True         | .bmp  | a8b18d9f277bc3b71fa80c35a63b836b255e75e1534567f009ae012903a72357 |
| listf.bmp     |            65 |             116 | True         | .bmp  | f36d6920923e372212bf7856d933eb0535b7285c072df10da11db8805e7ebe71 |
| progbar.bmp   |           106 |             108 | True         | .bmp  | 47abc406d26efb0926b4bcacd0e194dd55a0091df9c3eb4ad810f52d19f9d9c2 |

### gamelist.mrp

| member       |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:-------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr     |          1018 |            2490 | True         | .mr   | ff67eea7e6eed10e3871bd465953e7137001e2495b1eb13309c56db8659bfc7c |
| gamelist.ext |         58522 |           91532 | True         | .ext  | 70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414 |
| reg.ext      |           787 |             868 | True         | .ext  | e803033aae4175669f4759b6f372c0717ef72170ec1fd7ccbe26add4058197ef |
| cfg.bin      |           847 |            6898 | True         | .bin  | 12fdc4f2b83516e9f9cd5e2691dfe394243c2101ca1ad3c8ad14b68edf791042 |
| 1.bmp        |           446 |             800 | True         | .bmp  | bb4a0b2b84224b18d79ec233ef4b10ca46074765fb9c7cf177929bc318e08ec2 |
| 2.bmp        |           290 |             800 | True         | .bmp  | 44693ea7a279f27a6b1729fb8895eed418aac17aa8e1082f36a8140b774ad5a6 |
| 3.bmp        |           287 |             800 | True         | .bmp  | c43f257d9985734aad039632b3f34d8aa5754bd690bcbf928e8dd10efefbea83 |
| 4.bmp        |           259 |             800 | True         | .bmp  | 8ce6ec079a361f8bdb6eb93e082457752908e27f3bf56469be7aff43b6cb3d49 |
| 5.bmp        |           289 |             800 | True         | .bmp  | 79c1724bf3dc7cc035b5234733b4f7d5437f243688b30face5bc8029b812a0a6 |
| 6.bmp        |           299 |             800 | True         | .bmp  | 93c65d72a3079ed77df4ffde857d9246a272a6c2447d4d7df668bcb1998aefb5 |
| border.bmp   |           443 |            7920 | True         | .bmp  | cb83b285a8dc691625fd9175b24759326b6acdb4fb86f0ef1eaef9a335362ddb |
| dbboard.bmp  |            40 |             100 | True         | .bmp  | 143ccd38a69f226900eaee45271ab9284201ed1adb85fb575dd04b503e1232d7 |
| dbbot.bmp    |           140 |             384 | True         | .bmp  | 03e223c308e8fdc3f545fe552afa6a822cb27a87ec0bbc1b54b7cc30fae4b13d |
| dbhead.bmp   |           402 |            2112 | True         | .bmp  | f3f4d9d0ff03160d2d617f203660fe2a4b6beccf4303bc78ca6d40d488fa9b13 |
| dbtitle.bmp  |           437 |            1596 | True         | .bmp  | 632a96600d7c9f0d3ee4da23c9d1e0ff82e2160fc2ef1927c5cdc789abf74010 |
| dl_list.bmp  |           560 |             800 | True         | .bmp  | eba610003d61d1cc71da4e99631bb26d372e2d106df31a2e3a35a8a0e86af28e |
| dload.bmp    |           855 |            3200 | True         | .bmp  | d063276af4944da2bc4425ad273f57844eb03ddd06bfcfd43ce4ec53c17effdb |
| dload1.bmp   |           150 |             338 | True         | .bmp  | b2695e3115d5ce8e31ade78a5292e96552a5579cdfff3cc59e387c3f6ade2ec1 |
| dtiele.bmp   |            47 |              80 | True         | .bmp  | 4b6f544efb9daf3aa23311c11095f9690b8fcdb733bf1c6d96bb5cd78b45ea02 |
| exit.bmp     |          2715 |           10296 | True         | .bmp  | b2d07814e86f4437d0e5aeb9b7aa5c53bbacb3e3f409e5610265bb867237059b |
| gamelist.bmp |           553 |             800 | True         | .bmp  | acc5c2b4ede669c440c531c8c689563f5cb1dbb2cbde8026ac7eccae00c2ce86 |
| head.bmp     |           225 |             448 | True         | .bmp  | 8b8f555664df55ac7628e3e82a2b1aea91942fd0b52eadb88ee8d919c32fb58e |
| new.bmp      |            62 |             336 | True         | .bmp  | 77e46fb23cb79ed1211b38824fe8ee5313809f5c173e89da8d0cd3c3f9bb3fc0 |
| title.bmp    |           152 |             840 | True         | .bmp  | a794d229e579b89238223aee84cd2fd188bf21c7f55ba6d0daf34fa5696b6480 |
| dload.gif    |          1350 |            1452 | True         | .gif  | a5c5a9f7c5fe259932d3648ca41320743712c93da941f6b60ebe3964a433f459 |
| msg.mid      |            99 |             109 | True         | .mid  | 4bd584ed65f38fd15fd9c127d582dcb857e443e55ca4b1a32d0f6e5a0bab226a |

## 10. cfg.bin path records

| cfg                             |   path_off | path                 | pre16_hex                        | post48_hex                                                                                                                               |   pre_minus8_u32le_a |   pre_minus4_u32le_b |
|:--------------------------------|-----------:|:---------------------|:---------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------|---------------------:|---------------------:|
| 冒泡游戏320480/网游/gwy/cfg.bin |         14 | gwy/roomlist.mrp     | 00043338000600151a00000003e8     | 6777792f726f6f6d6c6973742e6d727000000000000000000000000000000000000000000000000000001100000008096777792f67627277636f72652e6d7270         |              1709312 |           3892510720 |
| 冒泡游戏320480/网游/gwy/cfg.bin |         62 | gwy/gbrwcore.mrp     | 00000000000000000000110000000809 | 6777792f67627277636f72652e6d727000000000000000000000000000000000000000000000000000001110000007d96777792f676272777368656c6c2e6d72         |              1114112 |            151519232 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        110 | gwy/gbrwshell.mrp    | 000000000000000000001110000007d9 | 6777792f676272777368656c6c2e6d7270000000000000000000000000000000000000000000000000002200000000206777792f666f6e742e6d72700000000000       |            269549568 |           3641114624 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        158 | gwy/font.mrp         | 00000000000000000000220000000020 | 6777792f666f6e742e6d72700000000000000000000000000000000000000000000000000000000000003d00000004b46777792f736d736368617267                 |              2228224 |            536870912 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        206 | gwy/smscharge.mrp    | 000000000000000000003d00000004b4 | 6777792f736d736368617267652e6d7270000000000000000000000000000000000000000000000000003e00000004b26777792f6469726563747061792e6d7270       |              3997696 |           3020161024 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        254 | gwy/directpay.mrp    | 000000000000000000003e00000004b2 | 6777792f6469726563747061792e6d727000000000000000000000000000000000000000000000000000ce00000000436777792f776d327065742e6d7270000000       |              4063232 |           2986606592 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        302 | gwy/wm2pet.mrp       | 00000000000000000000ce0000000043 | 6777792f776d327065742e6d727000000000000000000000000000000000000000000000000000000000ce0e000000436777792f776d322e6d7270000000             |             13500416 |           1124073472 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        350 | gwy/wm2.mrp          | 00000000000000000000ce0e00000043 | 6777792f776d322e6d7270000000000000000000000000000000000000000000000000000000000000003100000003e96777792f63617264636861                   |            248381440 |           1124073472 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        398 | gwy/cardcharge.mrp   | 000000000000000000003100000003e9 | 6777792f636172646368617267652e6d7270000000000000000000000000000000000000000000000000e990000003eb6777792f736d73626173652e6d7270000000     |              3211264 |           3909287936 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        446 | gwy/smsbase.mrp      | 00000000000000000000e990000003eb | 6777792f736d73626173652e6d72700000000000000000000000000000000000000000000000000000003e800000000f6777792f6361636c6f74746572792e           |           2431188992 |           3942842368 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        494 | gwy/caclottery.mrp   | 000000000000000000003e800000000f | 6777792f6361636c6f74746572792e6d72700000000000000000000000000000000000000000000000001c00000000526777792f656d626672642e6d727000000000     |           2151546880 |            251658240 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        542 | gwy/embfrd.mrp       | 000000000000000000001c0000000052 | 6777792f656d626672642e6d7270000000000000000000000000000000000000000000000000000000003200000000236777792f6361636c6f6262792e6d             |              1835008 |           1375731712 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        590 | gwy/caclobby.mrp     | 00000000000000000000320000000023 | 6777792f6361636c6f6262792e6d727000000000000000000000000000000000000000000000000000003500000000236777792f6361636368617267652e6d72         |              3276800 |            587202560 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        638 | gwy/caccharge.mrp    | 00000000000000000000350000000023 | 6777792f6361636368617267652e6d7270000000000000000000000000000000000000000000000000003380000000016777792f63616364647a616e692e6d7270       |              3473408 |            587202560 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        686 | gwy/cacddzani.mrp    | 00000000000000000000338000000001 | 6777792f63616364647a616e692e6d7270000000000000000000000000000000000000000000000000003280000000016777792f6361637075626c69632e6d7270       |           2150825984 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        734 | gwy/cacpublic.mrp    | 00000000000000000000328000000001 | 6777792f6361637075626c69632e6d7270000000000000000000000000000000000000000000000000003b000000000b6777792f6361636d617463682e6d727000       |           2150760448 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        782 | gwy/cacmatch.mrp     | 000000000000000000003b000000000b | 6777792f6361636d617463682e6d727000000000000000000000000000000000000000000000000000003600000000016777792f6361636d6f6e657972616e6b         |              3866624 |            184549376 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        830 | gwy/cacmoneyrank.mrp | 00000000000000000000360000000001 | 6777792f6361636d6f6e657972616e6b2e6d7270000000000000000000000000000000000000000000003700000000016777792f6361636d73672e6d7270000000000000 |              3538944 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        878 | gwy/cacmsg.mrp       | 00000000000000000000370000000001 | 6777792f6361636d73672e6d7270000000000000000000000000000000000000000000000000000000003400000000016777792f63616370726f66696c65             |              3604480 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        926 | gwy/cacprofile.mrp   | 00000000000000000000340000000001 | 6777792f63616370726f66696c652e6d72700000000000000000000000000000000000000000000000003800000000016777792f63616369737375652e6d72700000     |              3407872 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |        974 | gwy/cacissue.mrp     | 00000000000000000000380000000001 | 6777792f63616369737375652e6d7270000000000000000000000000000000000000000000000000000000000000001a00000000000000000000000000000000         |              3670016 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       1227 | gwy/xaqd.mrp         | 0000000001e8e0000000020000000005 | 6777792f786171642e6d72700000000000000000000000000000000000000000000000000000000003050607ffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       1499 | gwy/sanguo.mrp       | 00000000018500000000020000000005 | 6777792f73616e67756f2e6d7270000000000000000000000000000000000000000000000000000003080409ffffffffffffffffffffffffffffffff0000             |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       1771 | gwy/gkdxy.mrp        | 0000000001e9c0000000020000000001 | 6777792f676b6478792e6d72700000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff00               |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       2043 | gwy/zsol.mrp         | 0000000001e72a000000020000000001 | 6777792f7a736f6c2e6d72700000000000000000000000000000000000000000000000000000000003050409ffffffffffffffffffffffffffffffff                 |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       2315 | gwy/wxjwq.mrp        | 0000000001e5c0000000020000000005 | 6777792f77786a77712e6d72700000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff00               |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       2587 | gwy/ssjx.mrp         | 0000000001e440000000020000000005 | 6777792f73736a782e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       2859 | gwy/sanguo.mrp       | 00000000018500000000020000651005 | 6777792f73616e67756f2e6d7270000000000000000000000000000000000000000000000000000003080409ffffffffffffffffffffffffffffffff0000             |               131072 |             84960512 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       3131 | gwy/xyol.mrp         | 0000000001e5b0000000020000000005 | 6777792f78796f6c2e6d7270000000000000000000000000000000000000000000000000000000000803ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       3403 | gwy/yxlm.mrp         | 0000000001e7a0000000020000000005 | 6777792f79786c6d2e6d7270000000000000000000000000000000000000000000000000000000000803ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       4219 | gwy/ajss.mrp         | 0000000001e530000000020000000010 | 6777792f616a73732e6d7270000000000000000000000000000000000000000000000000000000000503ffffffffffffffffffffffffffffffffffff                 |               131072 |            268435456 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       4491 | gwy/spacetime.mrp    | 0000000001d600000000020000000005 | 6777792f737061636574696d652e6d727000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff0000000000       |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       4763 | gwy/tlbb.mrp         | 0000000001e800000000020000000005 | 6777792f746c62622e6d72700000000000000000000000000000000000000000000000000000000003050409ffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       5035 | gwy/tyol.mrp         | 0000000001e6d0000000020000000010 | 6777792f74796f6c2e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |            268435456 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       5307 | gwy/smd.mrp          | 0000000001e890000000020000000001 | 6777792f736d642e6d727000000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffff                   |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       5579 | gwy/wapgame.mrp      | 00000000012a01000000890000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              8978432 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       5851 | gwy/cacddz.mrp       | 00000000013202c600010100061a8101 | 6777792f63616364647a2e6d727000000000000000000000000000000000000000000000000000000a0b0c030d0e0f04090805ffffffffffffffffff0000             |                65792 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       6123 | gwy/sgmj.mrp         | 0000000001e960000000020000000010 | 6777792f73676d6a2e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |            268435456 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       6395 | gwy/wapgame.mrp      | 00000000012a01000000010000b31100 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |                65536 |              1159936 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       7755 | gwy/wapgame.mrp      | 00000000012a01000000a30000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |             10682368 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       8027 | gwy/assg.mrp         | 0000000001e950000000020000000005 | 6777792f617373672e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       8831 | gwy/yxlm.mrp         | 0000000001e7a0000000020000000005 | 6777792f79786c6d2e6d7270000000000000000000000000000000000000000000000000000000000803ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       9095 | gwy/smd.mrp          | 0000000001e890000000020000000001 | 6777792f736d642e6d727000000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffff                   |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       9359 | gwy/wapgame.mrp      | 00000000012a01000000010000b31100 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |                65536 |              1159936 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       9623 | gwy/wapgame.mrp      | 00000000012a01000000010000b30200 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |                65536 |               176896 |
| 冒泡游戏320480/网游/gwy/cfg.bin |       9887 | gwy/assg.mrp         | 0000000001e950000000020000000005 | 6777792f617373672e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      10151 | gwy/bbjq.mrp         | 0000000001e8d0000000020000000005 | 6777792f62626a712e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |             83886080 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      10415 | gwy/wapgame.mrp      | 00000000012a01000687e30000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |             14911238 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      10679 | gwy/gkdxy.mrp        | 0000000001e9c0000000020000000001 | 6777792f676b6478792e6d72700000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff00               |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      10943 | gwy/jjfb.mrp         | 0000000001e200000000020000000001 | 6777792f6a6a66622e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff                 |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      11207 | gwy/wapgame.mrp      | 00000000012a01000626910000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              9512454 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      11471 | gwy/wapgame.mrp      | 00000000012a01000626710000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              7415302 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      11735 | gwy/wapgame.mrp      | 00000000012a01000626690000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              6891014 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      13059 | gwy/wapgame.mrp      | 00000000012a010000271d0000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              1910528 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      13323 | gwy/wapgame.mrp      | 00000000012a010000006a0000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              6946816 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      13587 | gwy/wapgame.mrp      | 00000000012a010000009e0000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |             10354688 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      13851 | gwy/wapgame.mrp      | 00000000012a010006266e0000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              7218694 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      14119 | gwy/cacddz.mrp       | 00000000013202c600010100061a8101 | 6777792f63616364647a2e6d727000000000000000000000000000000000000000000000000000000a0b0c030d0e0f04090805ffffffffffffffffff0000             |                65792 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      14383 | gwy/cacgang5.mrp     | 00000000013202c600040000061a8101 | 6777792f63616367616e67352e6d72700000000000000000000000000000000000000000000000000b0c030d0f040908050affffffffffffffffffff00000000         |                 1024 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      14647 | gwy/caczjh.mrp       | 00000000013202c6000d0200061a8101 | 6777792f6361637a6a682e6d727000000000000000000000000000000000000000000000000000000b0c030d0f04090805ffffffffffffffffffffff0000             |               134400 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      14911 | gwy/cacnn.mrp        | 00000000013202c600050000061a8101 | 6777792f6361636e6e2e6d72700000000000000000000000000000000000000000000000000000000b0c030d0f04090805ffffffffffffffffffffff00               |                 1280 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      15175 | gwy/caccharge.mrp    | 000000000135000000000200061a8101 | 6777792f6361636368617267652e6d72700000000000000000000000000000000000000000000000030f04090805ffffffffffffffffffffffffffff0000000000       |               131072 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      15439 | gwy/caclottery.mrp   | 00000000013e83000003f000061a8101 | 6777792f6361636c6f74746572792e6d7270000000000000000000000000000000000000000000000f0d0408030509ffffffffffffffffffffffffff000000000000     |             15729408 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      15703 | gwy/caclottery.mrp   | 00000000013e800000000200061a8101 | 6777792f6361636c6f74746572792e6d7270000000000000000000000000000000000000000000000f0d0409080305ffffffffffffffffffffffffff000000000000     |               131072 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      15967 | gwy/cacachmt.mrp     | 00000000013480000000020000000001 | 6777792f6361636163686d742e6d727000000000000000000000000000000000000000000000000003ffffffffffffffffffffffffffffffffffffff00000000         |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      16231 | gwy/cacprize.mrp     | 00000000013a00000000020000000001 | 6777792f6361637072697a652e6d7270000000000000000000000000000000000000000000000000030d04090f0805ffffffffffffffffffffffffff00000000         |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      16495 | gwy/cacddz.mrp       | 00000000013b02c8000101000003f001 | 6777792f63616364647a2e6d727000000000000000000000000000000000000000000000000000000b10030d0e0f04090805ffffffffffffffffffff0000             |                65792 |             32506624 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      16759 | gwy/wapgame.mrp      | 00000000012a01000687f40000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |             16025350 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      17023 | gwy/cacxq.mrp        | 00000000013202c600030000061a8101 | 6777792f63616378712e6d72700000000000000000000000000000000000000000000000000000000b0c030d0f040908ffffffffffffffffffffffff00               |                  768 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      17287 | gwy/cacermj.mrp      | 00000000013202c600070000061a8101 | 6777792f63616365726d6a2e6d7270000000000000000000000000000000000000000000000000000b0c030d0f04090805ffffffffffffffffffffff000000           |                 1792 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      17551 | gwy/cacsk.mrp        | 00000000013202c600090200061a8101 | 6777792f636163736b2e6d72700000000000000000000000000000000000000000000000000000000b0c030d0f04090805ffffffffffffffffffffff00               |               133376 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      17815 | gwy/cacxq.mrp        | 00000000013b02c800030000061a8101 | 6777792f63616378712e6d72700000000000000000000000000000000000000000000000000000000b10030d0f04090805ffffffffffffffffffffff00               |                  768 |             25238022 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      18079 | gwy/cacusr.mrp       | 00000000013580000000020000000001 | 6777792f6361637573722e6d727000000000000000000000000000000000000000000000000000000f0311121314ffffffffffffffffffffffffffff0000             |               131072 |             16777216 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      18347 | gwy/wapgame.mrp      | 00000000012a010000271d0000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              1910528 |             15774464 |
| 冒泡游戏320480/网游/gwy/cfg.bin |      18611 | gwy/wapgame.mrp      | 00000000012a01000000980000b3f000 | 6777792f77617067616d652e6d727000000000000000000000000000000000000000000000000000000102030409ffffffffffffffffffffffffffff000000           |              9961472 |             15774464 |

## 11. start.mr hash groups

| sha256                                                           |   count | members                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
|:-----------------------------------------------------------------|--------:|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| e318e580d789041a524a656038fcf38c6be35f9abba48902c8d4580ea8563ed7 |      95 | ajss.mrp|assg.mrp|bbjq.mrp|gkdxy.mrp|jjfbol/attack.mrp|jjfbol/bigWorldMap.mrp|jjfbol/bmm1.mrp|jjfbol/bmm2.mrp|jjfbol/bmm3.mrp|jjfbol/bmm4.mrp|jjfbol/bmm5.mrp|jjfbol/bmm6.mrp|jjfbol/bmm7.mrp|jjfbol/bmm8.mrp|jjfbol/cannon1.mrp|jjfbol/chi.mrp|jjfbol/default.mrp|jjfbol/default2.mrp|jjfbol/default3.mrp|jjfbol/devil1.mrp|jjfbol/devil2.mrp|jjfbol/devil4.mrp|jjfbol/dong1.mrp|jjfbol/dongimage.mrp|jjfbol/downimage1.mrp|jjfbol/downimage2.mrp|jjfbol/downimage3.mrp|jjfbol/gun1.mrp|jjfbol/gun2.mrp|jjfbol/gunimage.mrp|jjfbol/hint.mrp|jjfbol/itemType.mrp|jjfbol/man1.mrp|jjfbol/man2.mrp|jjfbol/man3.mrp|jjfbol/mantecimg.mrp|jjfbol/mapimage1.mrp|jjfbol/mapimage2.mrp|jjfbol/mapman.mrp|jjfbol/mapValue.mrp|jjfbol/monster.mrp|jjfbol/monster1.mrp|jjfbol/monster10.mrp|jjfbol/monster2.mrp|jjfbol/monster3.mrp|jjfbol/monster4.mrp|jjfbol/monster5.mrp|jjfbol/monster6.mrp|jjfbol/monster7.mrp|jjfbol/monster8.mrp|jjfbol/monster9.mrp|jjfbol/mti.mrp|jjfbol/newzuojia.mrp|jjfbol/state1.mrp|jjfbol/tecbuff.mrp|jjfbol/vmimage.mrp|jjfbol/weapon1.mrp|jjfbol/weaponimage.mrp|jjfbol/wing1.mrp|jjfbol/wing2.mrp|jjfbol/wingimage.mrp|jjfbol/xg1.mrp|jjfbol/xg2.mrp|mhxx/mm_320x480/actorzj_mm480.mrp|mhxx/mm_320x480/actor_mm480.mrp|mhxx/mm_320x480/data_mm480.mrp|mhxx/mm_320x480/equips_mm480.mrp|mhxx/mm_320x480/fight_mm480.mrp|mhxx/mm_320x480/goods1_mm480.mrp|mhxx/mm_320x480/goods2_mm480.mrp|mhxx/mm_320x480/script_mm480.mrp|sgmj.mrp|smd.mrp|spacetime.mrp|ssjx/effect.mrp|ssjx/hero.mrp|ssjx/item.mrp|ssjx/map.mrp|ssjx/monster.mrp|ssjx.mrp|tlbb/320x480/actorzj_m480.mrp|tlbb/320x480/common_m480.mrp|tlbb/320x480/data_m480.mrp|tlbb/320x480/equips_m480.mrp|tlbb/320x480/fight_m480.mrp|tlbb/320x480/goods_m480.mrp|tlbb/320x480/script_m480.mrp|tlbb.mrp|tyol.mrp|wm2.mrp|wm2pet.mrp|xaqd.mrp|xyol.mrp|yxlm.mrp|zsol.mrp |
| 7fc8d5412ac5d932196ebd2b1f22b042965442057f604371f3025d072e94c5fb |      12 | caccharge.mrp|cacddz.mrp|cacddzani.mrp|caclobby.mrp|caclottery.mrp|cacpublic.mrp|dload.mrp|embfrd.mrp|gbrwshell.mrp|gui.mrp|hotchat.mrp|wapgame.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ff67eea7e6eed10e3871bd465953e7137001e2495b1eb13309c56db8659bfc7c |      12 | directpay.mrp|gamelist.mrp|gbrwcore.mrp|pmsg.mrp|reglogin.mrp|resmng.mrp|rollscr.mrp|roomlist.mrp|smsbase.mrp|smscharge.mrp|svrctrl.mrp|vdload.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| 08be35830b3ba895ce395a1b3091c8cf7bd0e985d5407ccea5233e843b5d5221 |       6 | xjwq/res/mrp/mmochat_res1.mrp|xjwq/res/mrp/mmochat_res2.mrp|xjwq/res/mrp/mmochat_res3.mrp|xjwq/res/mrp/mmochat_res4.mrp|xjwq/res/mrp/mmochat_res5.mrp|xjwq/res/mrp/mmochat_res6.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05 |       2 | jjfb.mrp|wxjwq.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| aa518953e32491eb345816181d5f9b9d65b5da907a2625f67e016ea141ed8b8f |       1 | cardcharge.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| 343197ddca55c015ec3ba917820b0f3dc5aff4da3e076a09485b3483744fbab6 |       1 | font.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| 5addb579b1e95ce496585518b00bbba617eca8799726a33df01daae4630efadb |       1 | sanguo.mrp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

## 12. reg primary groups

| primary        |   count | members                                                                                                                                                                             |
|:---------------|--------:|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| mrpmaker.ext   |       6 | xjwq/res/mrp/mmochat_res1.mrp|xjwq/res/mrp/mmochat_res2.mrp|xjwq/res/mrp/mmochat_res3.mrp|xjwq/res/mrp/mmochat_res4.mrp|xjwq/res/mrp/mmochat_res5.mrp|xjwq/res/mrp/mmochat_res6.mrp |
| dream.ext      |       2 | tlbb.mrp|zsol.mrp                                                                                                                                                                   |
| ajss.ext       |       1 | ajss.mrp                                                                                                                                                                            |
| assg.ext       |       1 | assg.mrp                                                                                                                                                                            |
| tlol.ext       |       1 | bbjq.mrp                                                                                                                                                                            |
| caccharge.ext  |       1 | caccharge.mrp                                                                                                                                                                       |
| cacddz.ext     |       1 | cacddz.mrp                                                                                                                                                                          |
| cacddzani.ext  |       1 | cacddzani.mrp                                                                                                                                                                       |
| caclobby.ext   |       1 | caclobby.mrp                                                                                                                                                                        |
| caclottery.ext |       1 | caclottery.mrp                                                                                                                                                                      |
| cacpublic.ext  |       1 | cacpublic.mrp                                                                                                                                                                       |
| cardcharge.ext |       1 | cardcharge.mrp                                                                                                                                                                      |
| directpay.ext  |       1 | directpay.mrp                                                                                                                                                                       |
| download.ext   |       1 | dload.mrp                                                                                                                                                                           |
| embfrd.ext     |       1 | embfrd.mrp                                                                                                                                                                          |
| font.ext       |       1 | font.mrp                                                                                                                                                                            |
| gamelist.ext   |       1 | gamelist.mrp                                                                                                                                                                        |
| gbrwcore.ext   |       1 | gbrwcore.mrp                                                                                                                                                                        |
| gbrwshell.ext  |       1 | gbrwshell.mrp                                                                                                                                                                       |
| gkdxy.ext      |       1 | gkdxy.mrp                                                                                                                                                                           |
| gui.ext        |       1 | gui.mrp                                                                                                                                                                             |
| hotchat.ext    |       1 | hotchat.mrp                                                                                                                                                                         |
| robotol.ext    |       1 | jjfb.mrp                                                                                                                                                                            |
| pmsg.ext       |       1 | pmsg.mrp                                                                                                                                                                            |
| reglogin.ext   |       1 | reglogin.mrp                                                                                                                                                                        |
| resmng.ext     |       1 | resmng.mrp                                                                                                                                                                          |
| roll.ext       |       1 | rollscr.mrp                                                                                                                                                                         |
| roomlist.ext   |       1 | roomlist.mrp                                                                                                                                                                        |
| sanguo.ext     |       1 | sanguo.mrp                                                                                                                                                                          |
| sgmj.ext       |       1 | sgmj.mrp                                                                                                                                                                            |
| smd.ext        |       1 | smd.mrp                                                                                                                                                                             |
| smsbase.ext    |       1 | smsbase.mrp                                                                                                                                                                         |
| smscharge.ext  |       1 | smscharge.mrp                                                                                                                                                                       |
| spacetime.ext  |       1 | spacetime.mrp                                                                                                                                                                       |
| gameonline.ext |       1 | ssjx.mrp                                                                                                                                                                            |
| svrctrl.ext    |       1 | svrctrl.mrp                                                                                                                                                                         |
| tyol.ext       |       1 | tyol.mrp                                                                                                                                                                            |
| vdload.ext     |       1 | vdload.mrp                                                                                                                                                                          |
| wapgame.ext    |       1 | wapgame.mrp                                                                                                                                                                         |
| wm1_help.ext   |       1 | wm2.mrp                                                                                                                                                                             |
| wmpk.ext       |       1 | wm2pet.mrp                                                                                                                                                                          |
| mmochat.ext    |       1 | wxjwq.mrp                                                                                                                                                                           |
| wm1.ext        |       1 | xaqd.mrp                                                                                                                                                                            |
| xiyou.ext      |       1 | xyol.mrp                                                                                                                                                                            |
| xqj.ext        |       1 | yxlm.mrp                                                                                                                                                                            |
