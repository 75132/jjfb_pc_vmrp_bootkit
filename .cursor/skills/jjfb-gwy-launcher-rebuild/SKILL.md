# Skill: JJFB / GWY Independent Launcher Rebuild

## Purpose

Use this skill when implementing, reviewing or debugging the clean GWY/MRP launcher. It keeps work centered on shell-launch compatibility and prevents regression into target memory-state forcing.

## Inputs to read first

1. `00_READ_ME_FIRST.md`
2. `docs/03_TARGET_ARCHITECTURE.md`
3. Current phase in `docs/08_REDEVELOPMENT_ROADMAP.md`
4. Exact task in `docs/15_FIRST_30_TASKS.md`
5. Relevant golden evidence under `evidence/`
6. `profiles/jjfb.example.json` and its schema

## Decision procedure

### Step 1: classify the proposed change

Choose exactly one:

- FORMAT: parse MRP/cfg/reg.ext
- DESCRIPTOR: build/validate launch contract
- VFS: resolve/read/write paths
- RUNTIME: map/execute guest code
- EXT: resolve/load/register modules
- PLATFORM: provide ABI service
- SCHEDULER: lifecycle/timer/event/callback dispatch
- DISPLAY/INPUT
- NETWORK
- RESEARCH_ONLY

If the change includes a fixed JJFB address, ERW offset, game-state write or visual forcing, classify it as RESEARCH_ONLY and move it to `legacy_lab`; do not modify clean core.

### Step 2: state evidence level

For every numeric code or behavior, label source:

```text
DOCUMENTED | CROSS_TARGET | TARGET_OBSERVED | HYPOTHESIS
```

Core defaults require DOCUMENTED or CROSS_TARGET. TARGET_OBSERVED may become a profile capability. HYPOTHESIS requires a discriminating experiment first.

### Step 3: define a falsifiable completion condition

Bad:

```text
The game seems to progress.
```

Good:

```text
Given the original JJFB archive, exact lookup misses cfunction.ext,
profile alias resolves robotol.ext, decoded length is 253420,
target hash remains unchanged, and the loader records a third EXT registration.
```

### Step 4: add test before integration

- parser: golden fixture and malformed corpus
- VFS: normalization/traversal/overlay tests
- scheduler: fake clock and reentrancy test
- service: pointer/length/error tests
- integration: original hash and expected loader milestones

### Step 5: implement minimum generic behavior

No target-specific branching in core. Use profile/resolver/registry abstractions.

### Step 6: run the gates

```text
build
unit tests
integration test
audit_launcher_core.py
hash verification
```

### Step 7: report and stop

Do not silently begin the next phase.

## Debugging hierarchy

When launch fails, inspect in this order:

1. resource root and immutable hash
2. cfg/profile descriptor mismatch
3. VFS guest→host path
4. archive member exact/alias resolution
5. decompression and memory mapping
6. EXT registration and entry return
7. platform service arguments/returns
8. scheduler registrations and queue
9. display/input
10. network/business-server availability

Do not skip directly to target memory inspection.

## Known JJFB validation vector

```text
target: gwy/jjfb.mrp
sha256: 52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036
appid/appver: 400101/12
members: start.mr 1514, mrc_loader.ext 219, robotol.ext 161178
alias: cfunction.ext -> robotol.ext
loader init sequence: 6, 8, 0
```

This vector validates generic modules; it does not authorize fixed-address target hacks.

## Required output template

```text
Phase / task:
Change classification:
Evidence level:
Files changed:
Tests added:
Commands run:
Results:
Anti-drift audit:
Remaining unknowns:
Stop/next task:
```
