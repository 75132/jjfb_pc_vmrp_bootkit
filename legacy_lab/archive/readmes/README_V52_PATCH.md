# JJFB v52 — MRP Member Alias

v52 continues the confirmed v51 route. It does not return to UI experiments and does not alter `gwy/jjfb.mrp`.

## Confirmed input condition

The original package reaches:

```text
start.mr (1514)
→ mrc_loader.ext (219)
→ request cfunction.ext
→ read-file error 3006
```

The MRP index contains `robotol.ext` (compressed length `161178`) and does not contain `cfunction.ext`.

## v52 change

At the second-stage `_mr_c_function_new` callback, the host verifies the canonical loader layout and changes only the unpacked loader request literal in guest memory:

```text
cfunction.ext → robotol.ext
```

The original MRP file and its index are unchanged. The runner checks the MRP SHA-256 before and after execution.

v52 also adds an 801 guard: host `_strCom` codes `6 → 8 → 0` run only after a new post-alias EXT helper has registered. This prevents the v51 false positive where mrc_loader itself returned `mrc_init=0` and armed the timer.

## Apply and run

Extract the ZIP into the existing project root, preserving directories, then run:

```powershell
.\RUN_V52_MRP_MEMBER_ALIAS.ps1 -Seconds 25
```

Primary report:

```text
reports\v52_mrp_member_alias_run_result.md
```

Expected proof sequence:

```text
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] patched ... cfunction.ext target=robotol.ext
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ...
[JJFB_801_GUARD] robotol_loaded=1 ... action=run_host_801
[JJFB_801] host mrc_init(0) ret=0
```

If the alias is applied but `161178` is absent, retain the raw log; the next blocker remains within mrc_loader after the rewritten request. If `161178` appears but `mrc_init` is nonzero, the route has advanced to robotol `_strCom 800/801` auditing.
