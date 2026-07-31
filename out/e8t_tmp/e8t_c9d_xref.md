# E8T C9D writer xref

Gate: R9+0xC9D must be ==1 (LDRSB @ 0x3066BC).

## Exact STRB @ C9D
- `0x30AA46` `STRB r4,[r0,#0x1]` src=MOVS#0 class=writes_0 fn=0x30A9EC callers=2 ['0x30AF8A', '0x30DF78']
- `0x3115BA` `STRB r7,[r0,#0x1]` src=MOVS#0 (r7) class=writes_0 fn=0x311546 callers=0 []

## Covering STR/STRH (may or may not set C9D)

- `0x2DCF76` STR src=r0 class=writes_variable_nonzero note=word store; C9D is byte2; MOVS#1 only sets byte0
- `0x2E3A7C` STR src=MOVS#1 class=writes_1 note=word store; C9D is byte1; MOVS#1 only sets byte0
- `0x2E3A84` STR src=MOVS#0 class=writes_0 note=word store; C9D is byte1; MOVS#1 only sets byte0

## Adjacent C9C=#1 (NOT C9D)

- `0x2E3A68` fn=0x2E2520 STRB #1 @ C9C; gate reads C9D — does not unlock idle C9D
- `0x2FB008` fn=0x2FAFFC STRB #1 @ C9C; gate reads C9D — does not unlock idle C9D

## UI-init 0x2E4788 state requirement

- rejected_states: [38, 46, 69, 252, 300]
- reject_300: True
- state CMPs: [{'pc': '0x2E4796', 'reg': 0, 'imm': 0}, {'pc': '0x2E47B2', 'reg': 0, 'imm': 0}, {'pc': '0x2E47C4', 'reg': 0, 'imm': 252}, {'pc': '0x2E47C8', 'reg': 0, 'imm': 38}, {'pc': '0x2E47CC', 'reg': 0, 'imm': 46}, {'pc': '0x2E47D0', 'reg': 0, 'imm': 69}, {'pc': '0x2E47DA', 'reg': 0, 'imm': 1}, {'pc': '0x2E47E4', 'reg': 6, 'imm': 191}, {'pc': '0x2E48D6', 'reg': 6, 'imm': 230}, {'pc': '0x2E494C', 'reg': 6, 'imm': 218}, {'pc': '0x2E49BA', 'reg': 6, 'imm': 108}, {'pc': '0x2E4A30', 'reg': 6, 'imm': 23}, {'pc': '0x2E4AAA', 'reg': 6, 'imm': 58}]
- callers: ['0x2E2F50', '0x2E39BE', '0x2E39C8', '0x2E39D2', '0x2E39DC', '0x2E3BB2', '0x2E3F7C']
- unlock BLs: ['0x2E4840->0x2FC8C0', 'UNLOCK_BL@2E4840', '0x2E48BC->0x2FC8C0', 'UNLOCK_BL@2E48BC', '0x2E4932->0x2FC8C0', 'UNLOCK_BL@2E4932', '0x2E49A8->0x2FC8C0', 'UNLOCK_BL@2E49A8']

## Verdict hint: `C9D_NONZERO_WRITER_NEVER_REACHED`

Exact STRB @ C9D are clears only (0x30AA46, 0x3115BA). No MOVS#1 STRB @ C9D in robotol.ext. C9C=#1 sites (0x2E3A68, 0x2FB008) do not satisfy gate LDRSB C9D. Nonzero C9D may require memcpy/other-module/path not yet found.
