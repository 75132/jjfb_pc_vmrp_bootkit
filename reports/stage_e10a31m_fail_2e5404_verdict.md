# Stage E10A-3.1m fail 0x2E5404 provenance

- **Mode**: `predicate` → `apply_once` (method0-enter apply)
- **Primary verdict**: `METHOD0_NEXT_PRECONDITION_FOUND`
- Also: `METHOD0_2E5404_FAIL_PREDICATE_FOUND`, `METHOD0_2E5404_COMMON_FAILURE_EPILOGUE`, `METHOD0_FAIL_SOURCE_SMSCFG_355_356`, `SMSCFG_355_356_FIELD_TYPE_IDENTIFIED`

## Answer

```text
0x2E5404 返回 -1 前，比较的是：
  SMSCFG+0x355 的 int16_le（覆盖 0x355/0x356 两字节）
  期望：1 <= value <= 0x1B2 (434)
  当前值：0 → CMP #0 / BLE → 公共失败尾声 0x2E5404
```

## Lane A — static

Window `0x2E53C0..0x2E5410`:

1. gwy strcmp pass
2. copy SMSCFG+**0x355** len=**2** → SP+0x48
3. `LDRSH` as signed halfword
4. Predicate A `@0x2E53F4`: `CMP r0,#0` / `BLE 0x2E5404` (fail if ≤0)
5. Predicate B `@0x2E5400`: r1=`0xFF+0xB3`=`0x1B2`; `CMP r0,r1` / `BLE success`; fallthrough fail if >0x1B2
6. `0x2E5404`: `ADDS r0,r5,#0` with r5=-1 (from earlier `MVNS r5,r4`) — **common failure epilogue**, then `B` exit

Artifacts: `out/e10a31m/fail_2e5404_annotated.txt`, `out/e10a31m/fail_2e5404_cfg.dot`

## Lane B/C — dynamic

| Item | Value |
|------|-------|
| predecessor_pc | `0x2E53F6` (BLE) |
| predicate_pc | `0x2E53F4` |
| lhs | int16 from SMSCFG+0x355 (=0) |
| rhs | imm 0 |
| branch_taken | 1 → fail |
| field type | **one int16_le**, not two uint8 |

SMSCFG reads at 0x355/0x356 both `00`, marked `CAUSAL_TO_0x2E5404`.

## Lane D/E

- Source: **SMS_CFG** (not launch-param / appinfo / etc.)
- Requirement: `reports/e10a31m_requirement.json`
- Next subsystem CSV: `SMS_CFG` causal

## Lane F — controlled apply

| Case | Apply | method0 | fail PC |
|------|-------|---------|---------|
| A | GPT+gwy only | -1 | **0x2E5404** |
| B | + int16@0x355=1 at **method0 enter** | -1 | **0x2E3FBA** |

- Gate cleared: `pred_a` not taken, `pred_b` success, `BL 0x2E5410` with `arg0=value+1=2`
- Verdict: **`METHOD0_NEXT_PRECONDITION_FOUND`**
- Note: bootstrap-time write of 0x355 crashed host before method0; apply moved to method0 enter

Profile updated (fingerprint-pinned):

```text
mythroad_mini_2011_smscfg_0x10E0_gpt349_gwy34c_i16_355
tags: GPT@0x349 + gwy@0x34C + int16_le@0x355=1
original_default_recovered=false
```

## Lane G

Skipped — method0 still ≠ 0. Next gate is around **`0x2E3FBA`**.

## Artifacts

- `reports/stage_e10a31m_fail_2e5404_verdict.md`
- `reports/e10a31m_fail_branch_trace.csv`
- `reports/e10a31m_smscfg_355_356_provenance.csv`
- `reports/e10a31m_requirement.json`
- `reports/e10a31m_next_subsystem_provenance.csv`
- `reports/e10a31m_requirement_ab_compare.csv`
- `out/e10a31m/fail_2e5404_annotated.txt`
- `out/e10a31m/fail_2e5404_cfg.dot`
- `RUN_E10A31M_FAIL_2E5404.ps1`
