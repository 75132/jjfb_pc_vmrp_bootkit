# P21 cfg36 Selection Verdict

## Bottom line

**Class: A**
P20-CLEAN freeze held: True
Primary evidence lane: A (research_assisted=0 product_valid=1)

## Evidence tiers

| Lane | fast_assist | gamelist | ERW iso | FIRE_EXT | cfg_open | cfg36_present | cfg36_selected |
|------|-------------|----------|---------|----------|----------|---------------|----------------|
| A natural | 0/0 | 1 | 1 | 6 | 0 | 0 | 0 |
| B assisted | 1/1 | 1 | 1 | 6 | 0 | 0 | 0 |

Same cfg stop A/B: True

## Five gates (must not collapse)

| Gate | Pass |
|------|------|
| CFG_FMT_MAPPED | 1 |
| CFG_FILE_OPENED | 0 |
| CFG_RECORD_READ | 0 |
| CFG36_RECORD_PRESENT | 0 |
| CFG36_SELECTED | 0 |

## P20 freeze checks

| Check | Pass |
|-------|------|
| gbrwcore entry | 1 |
| br_exit CONTINUE | 1 |
| gamelist entered | 1 |
| gamelist ERW isolated | 1 |
| 0x30D5D2 fault = 0 | True |
| forced 10140 = 0 | True |
| MRPGCMAP EMU_OK | 1 |

## Acceptance answers

```text
鏃?fast assist 鏄惁浠嶈繘鍏?gamelist锛?aGl
鏃?fast assist 鏄惁浠嶅埌杈剧浉鍚?cfg 鍋滅偣锛?aCfgStop

鐪熷疄 cfg 鏁版嵁婧愶細NONE_OPENED
鐪熷疄鎵撳紑璺緞锛?(Read-CsvHint (Join-Path C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports 'p21_cfg_file_io.csv') 'path')
cfg 鍒楄〃璁板綍鏁帮細0_or_unknown
鏄惁瀛樺湪瀹屾暣 cfg36 record锛?([bool]System.Collections.Specialized.OrderedDictionary.cfg36_present)
cfg36 Guest 鍦板潃锛?(if (System.Collections.Specialized.OrderedDictionary.cfg36_va_line -match 'guest=(0x[0-9A-Fa-f]+)') { System.Collections.Hashtable[1] } else { 'N/A' })
cfg36 source offset锛?(if (System.Collections.Specialized.OrderedDictionary.cfg36_va_line -match 'src_off=(\d+)') { System.Collections.Hashtable[1] } else { 'N/A' })

鍚姩鍙傛暟鏄惁琚?cfunction 瑙ｆ瀽锛?(if (9 -gt 0) { 'PARTIAL_OR_YES' } else { 'NO_OR_UNOBSERVED' })
瑙ｆ瀽缁撴灉鍐欏叆鍦板潃锛歴ee reports/p21_launch_param_provenance.csv
gamelist 鏄惁璇诲彇瑙ｆ瀽缁撴灉锛?(if (2 -gt 0) { 'YES' } else { 'NO_OR_UNOBSERVED' })
gwyblink 鐨勭湡瀹炶涔夛細observe_only_see_param_csv

cfg36 閫夋嫨璋撹瘝锛?(if (System.Collections.Specialized.OrderedDictionary.cfg36_present) { 'see selection_branches.csv' } else { 'N/A_no_record' })
褰撳墠澶辫触鐨勭涓€椤硅皳璇嶏細CFG_FILE_OPENED
璐熻矗婊¤冻璇ヨ皳璇嶇殑鑷劧鐢熶骇鑰咃細TBD_from_csv
鏄惁绛夊緟鐪熷疄鐢ㄦ埛杈撳叆锛?(if (A -match 'E') { 'LIKELY' } else { 'UNKNOWN' })

cfg36 鏄惁鐢?Guest 鑷劧閫変腑锛?([bool]System.Collections.Specialized.OrderedDictionary.cfg36_selected)
selected state 鍐欏叆 PC锛?(if (System.Collections.Specialized.OrderedDictionary.cfg36_selected) { 'see selection_branches.csv' } else { 'N/A' })
post-select 绗竴涓湡瀹炶涓猴細N/A
鏄惁鍑虹幇 Guest startGame 璋冪敤锛?([bool]System.Collections.Specialized.OrderedDictionary.startgame_call)
褰撳墠鍞竴闂ㄩ攣锛?class
```

## Notes

- `SHELL_PHASE_CFG_FMT_MAPPED` is **not** evidence of cfg36 load/select.
- G6b (dynamic startGame) remains deferred until CFG36_SELECTED.
- Lane B is `research_assisted=yes product_valid=no` if fast assist was on.

## Artifacts

- reports/p21_cfg36_selection_verdict.md
- reports/p21_cfg_file_io.csv
- reports/p21_cfg_record_inventory.csv
- reports/p21_launch_param_provenance.csv
- reports/p21_cfg_selection_branches.csv
- reports/p21_timer_state_diff.csv
- out/p21/p21_build_identity.txt
- research/runners/p21_run_cfg36_selection.ps1
