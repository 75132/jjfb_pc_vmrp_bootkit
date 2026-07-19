# v48 update-string xref (secondary)

Full string VA word scan remains low priority per v48 spec.

Runtime: `JJFB_STARTUP_STR] #` = 0 under natural, event scan, and FORCE_2EFC_TAIL.

Static: prior bands found 0 literal hits for check_update VAs; need wider map + pointer-table scan in a later pass after r4-path is unlocked.

No evidence yet that check-update UI is reachable without passing the `0x2EFAF2 r4!=0` gate first.
