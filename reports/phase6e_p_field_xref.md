# Phase 6E P+0xC field xref (live + heuristic)

Static guest image scan is limited without a dumped code image in this phase.
This report lists **executed** P-field accesses from live hooks.

## Live P reads

| off | val | pc | lr | executed |
|---|---|---|---|---|
| _(none)_ | | | | |

## Live P writes

| off | old | new | pc | lr | executed |
|---|---|---|---|---|---|
| _(none)_ | | | | | |

## P+0xC specific

- writes_seen (summary): 0
- reads_seen (summary): 1

If writes_seen=0 and a read of P+0xC returned 0 before `LDR [r0,#0x28]`,
the provider path never published mrc_extChunk on this launch.

