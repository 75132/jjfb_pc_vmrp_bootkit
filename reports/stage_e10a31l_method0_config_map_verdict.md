# Stage E10A-3.1l method0 config map

- **Mode**: `config_map` → `profile_ab` (two-round plan)
- **Primary observe verdict**: `GWY_COMPARE_SOURCE_SMSCFG`
- **Fingerprint**: `SMSCFG_PROFILE_FINGERPRINT_PINNED`
- **After applying proven tags**: `SMSCFG_REQUIREMENTS_PASSED_NEXT_SUBSYSTEM_FOUND`

## Round 1 — observe (no guessed writes)

### gwy lhs provenance

| Field | Value |
|-------|-------|
| lhs | `0x27FA30` (stack/work buffer) |
| rhs | `0x2E8458` (`"gwy\\0GPT\\0…"`) |
| classification | `STACK_FROM_SMSCFG` |
| cfg_offset | **844 (`0x34C`)** |
| copy | memcpy from `cfg_base+0x34C`, len=8 |
| verdict | **`GWY_COMPARE_SOURCE_SMSCFG`** |

Proven by data-flow (sms_cfg read → dst buffer → strcmp), **not** by rodata adjacency.

### SMSCFG read map (method0 window)

Reads clustered at `0x349`–`0x353` (GPT field + following `gwy` field).

### Ordered compare chain

1. **REQ_1** SMSCFG `0x349` len=3 expect `"GPT"` — first gate
2. **REQ_2** SMSCFG `0x34C` len=3 (read window 8) expect `"gwy"` — second gate

Artifacts:

- `reports/e10a31l_gwy_strcmp_provenance.csv`
- `reports/e10a31l_method0_smscfg_read_map.csv`
- `reports/e10a31l_method0_compare_chain.csv`
- `reports/e10a31l_required_platform_tags.json`
- `out/e10a31l/method0_config_gate_chain_annotated.txt`

## Round 2 — one-shot profile apply

Pinned profile (exact cfunction SHA256):

```text
mythroad_mini_2011_smscfg_0x10E0_gpt349_gwy34c
tags: GPT@0x349 + gwy@0x34C
original_default_recovered=false
```

Bootstrap log:

```text
gpt_hex=475054 gwy_hex=677779
SMSCFG_PROFILE_FINGERPRINT_PINNED
```

### Result

| Gate | Result |
|------|--------|
| GPT strcmp | **pass** (`lhs=GPT`) |
| gwy strcmp | **pass** (`lhs=gwy`) |
| old TRUE_FAIL `0xAC2E8` | **gone** (`saw_true_fail=0`) |
| method0 | still **-1** |
| new fail PC | **`0x2E5404`** (`RETURN_NEG1_IMMEDIATE` in gamelist) |

→ **`SMSCFG_REQUIREMENTS_PASSED_NEXT_SUBSYSTEM_FOUND`**

## What not to do next

- Do not keep adding one SMSCFG string per stage without a new batch map
- `cfg_validate` stays disabled until method0 returns 0
- Next: map preconditions around **`0x2E5404`** (may be non-SMSCFG)

## Env hygiene

- Runner no longer sets `GWY_PACKAGE_APPID` / `GWY_PACKAGE_APPVER` (MRP metadata only)
- Product profile selection requires exact cfunction SHA256 (`anyhash` only with `GWY_SMSCFG_ALLOW_ANYHASH=1`)
