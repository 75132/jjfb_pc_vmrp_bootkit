# Phase 6F — gwy startGame/runapp static chain (string xref)

Resource gwy root: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy`

Evidence: **TARGET_OBSERVED** string presence in packages; not proof of execution.

## `cfg.bin`

### member `cfg.bin` (1 hits)

| needle | offset | context |
|---|---|---|
| `gwy/jjfb.mrp` | `0x2ABF` | `................gwy/jjfb.mrp................................` |

## `gamelist.mrp`

### member `start.mr` (8 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0x2F3` | `trCom..Y........cfunction.ext.. .........................` |
| `_strCom` | `0x1DD` | `GetSysInfo......_strCom..!.............vmver...........` |
| `_strCom` | `0x2E1` | `._mr_c_buf......_strCom..Y........cfunction.ext.. .....` |
| `_strCom` | `0x5C6` | `i......iii......_strCom..!.......>.........}...........` |
| `_strCom` | `0x730` | `.....t_ret......_strCom..!.............................` |
| `_strCom` | `0x7D5` | `.....s_ret......_strCom..!.............................` |
| `_strCom` | `0x87A` | `.....r_ret......_strCom..!.............................` |
| `TestCom` | `0x555` | `times...........TestCom....................._t......str` |

### member `gamelist.ext` (14 hits)

| needle | offset | context |
|---|---|---|
| `gwyblink` | `0x14120` | `type=%d_nurl=%s_gwyblink....napptype=%d_nextid=%d_ncode=` |
| `gwyblink` | `0x14168` | `=%d_nmrpname=%s_gwyblink....%s/%s...cfunction.ext...star` |
| `gwyblink` | `0x141B0` | `.....%.s........gwyblink....napptype....entry...nextid..` |
| `napptype` | `0x1410C` | `b.......gwy.GPT.napptype=%d_nurl=%s_gwyblink....napptype` |
| `napptype` | `0x1412C` | `=%s_gwyblink....napptype=%d_nextid=%d_ncode=%d_narg=%d_n` |
| `napptype` | `0x141BC` | `....gwyblink....napptype....entry...nextid..ncode...narg` |
| `nextid` | `0x14138` | `....napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nm` |
| `nextid` | `0x141D0` | `type....entry...nextid..ncode...narg....narg1...nmrpna` |
| `ncode` | `0x14142` | `pe=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s` |
| `ncode` | `0x141D8` | `entry...nextid..ncode...narg....narg1...nmrpname....g` |
| `ncode` | `0x15FE5` | `/x-www-form-urlencoded;charset=UTF-8...%s %s HTTP/1.1` |
| `nmrpname` | `0x1415C` | `arg=%d_narg1=%d_nmrpname=%s_gwyblink....%s/%s...cfunctio` |
| `nmrpname` | `0x141F0` | `narg....narg1...nmrpname....gwy/%s.mrp..nappid..%s.mrp..` |
| `cfunction` | `0x1417C` | `link....%s/%s...cfunction.ext...start.mr.....%.s.%.s.....` |

## `gbrwcore.mrp`

### member `start.mr` (8 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0x2F3` | `trCom..Y........cfunction.ext.. .........................` |
| `_strCom` | `0x1DD` | `GetSysInfo......_strCom..!.............vmver...........` |
| `_strCom` | `0x2E1` | `._mr_c_buf......_strCom..Y........cfunction.ext.. .....` |
| `_strCom` | `0x5C6` | `i......iii......_strCom..!.......>.........}...........` |
| `_strCom` | `0x730` | `.....t_ret......_strCom..!.............................` |
| `_strCom` | `0x7D5` | `.....s_ret......_strCom..!.............................` |
| `_strCom` | `0x87A` | `.....r_ret......_strCom..!.............................` |
| `TestCom` | `0x555` | `times...........TestCom....................._t......str` |

### member `gbrwcore.ext` (11 hits)

| needle | offset | context |
|---|---|---|
| `startGame` | `0x223D8` | `lib.alert...lib.startGame...lib.exitbrw.lib.reportresult.` |
| `runapp` | `0x224C4` | `lib.loadurl.lib.runapp..FALSE...base64..get.%s&retcode` |
| `ncode` | `0x23D51` | `/x-www-form-urlencoded;charset=UTF-8...%s %s HTTP/1.1` |
| `cfunction` | `0x22DDC` | `>H..file:///....cfunction.ext...logo.ext....start.mr....s` |
| `download` | `0x224A8` | `lientInfo...lib.download....lib.loadurl.lib.runapp..FALS` |
| `download` | `0x22C38` | `....://.:...sky_download:........./.....>B..%s&nwid=%u&h` |
| `download` | `0x23924` | `ie..http/cookie.download....http....download/dwnlist.dat` |
| `download` | `0x23938` | `load....http....download/dwnlist.dat....gbrw/...[2......` |
| `isFileOnServerNewer` | `0x22458` | `checkmrpver.lib.isFileOnServerNewer.lib.getmrpver...lib.getFileVers` |
| `getFileVersion` | `0x22480` | `getmrpver...lib.getFileVersion..lib.getClientInfo...lib.downlo` |
| `getClientInfo` | `0x22494` | `ileVersion..lib.getClientInfo...lib.download....lib.loadurl.l` |

## `gbrwshell.mrp`

### member `start.mr` (8 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0x241` | `trCom..Y........cfunction.ext.. .........................` |
| `_strCom` | `0x168` | `GetSysInfo......_strCom..!.............vmver...........` |
| `_strCom` | `0x22F` | `._mr_c_buf......_strCom..Y........cfunction.ext.. .....` |
| `_strCom` | `0x514` | `i......iii......_strCom..!.......>.........}...........` |
| `_strCom` | `0x67E` | `.....t_ret......_strCom..!.............................` |
| `_strCom` | `0x723` | `.....s_ret......_strCom..!.............................` |
| `_strCom` | `0x7C8` | `.....r_ret......_strCom..!.............................` |
| `TestCom` | `0x4A3` | `times...........TestCom....................._t......str` |

### member `gbrwshell.ext` (3 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0x9BD0` | `...B..(.........cfunction.ext...logo.ext....shell.mr....s` |
| `download` | `0xAC51` | `on/sky-mrp.gbrw/download...Z:..%s/DOWNLOADLIST%d...gbrw/` |
| `download` | `0xAC79` | `ADLIST%d...gbrw/download...%s/TMPDOWNLOADLIST%d....MRPG.` |

## `vdload.mrp`

### member `start.mr` (8 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0x2F3` | `trCom..Y........cfunction.ext.. .........................` |
| `_strCom` | `0x1DD` | `GetSysInfo......_strCom..!.............vmver...........` |
| `_strCom` | `0x2E1` | `._mr_c_buf......_strCom..Y........cfunction.ext.. .....` |
| `_strCom` | `0x5C6` | `i......iii......_strCom..!.......>.........}...........` |
| `_strCom` | `0x730` | `.....t_ret......_strCom..!.............................` |
| `_strCom` | `0x7D5` | `.....s_ret......_strCom..!.............................` |
| `_strCom` | `0x87A` | `.....r_ret......_strCom..!.............................` |
| `TestCom` | `0x555` | `times...........TestCom....................._t......str` |

### member `vdload.ext` (3 hits)

| needle | offset | context |
|---|---|---|
| `simpleDownload` | `0x2F89` | `inueDownload.../simpleDownload.vld.spd.skymobiapp.com..MRPG...` |
| `continueDownload` | `0x2F75` | `%x%x.t........./continueDownload.../simpleDownload.vld.spd.skymo` |
| `dl_confirm` | `0x2F41` | `biapp.com:6009./dl_confirm.vld\%x%x%x../...\%x%x.i.\%x%x.t` |

## `jjfb.mrp`

### member `start.mr` (7 hits)

| needle | offset | context |
|---|---|---|
| `_strCom` | `0x3CC` | `.....close......_strCom...........vmver......string....` |
| `_strCom` | `0x5ED` | `._mr_c_buf......_strCom..Y........mrc_loader.ext.. ....` |
| `_strCom` | `0x973` | `i......iii......_strCom..!.......>.........}...........` |
| `_strCom` | `0xADD` | `.....t_ret......_strCom..!.............................` |
| `_strCom` | `0xB82` | `.....s_ret......_strCom..!.............................` |
| `_strCom` | `0xC27` | `.....r_ret......_strCom..!.............................` |
| `TestCom` | `0x902` | `times...........TestCom....................._t......str` |

### member `mrc_loader.ext` (1 hits)

| needle | offset | context |
|---|---|---|
| `cfunction` | `0xD4` | `....../.....0...cfunction.ext.......` |

### member `reg.ext` (1 hits)

| needle | offset | context |
|---|---|---|
| `robotol` | `0x838` | `................robotol.ext.moduletest.ext..mainmenumod` |

