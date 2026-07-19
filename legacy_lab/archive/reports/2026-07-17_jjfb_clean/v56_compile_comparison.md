# v56 C Syntax Comparison

The Linux host compiler cannot fully reproduce the Windows 32-bit build because the historical source tree expects v50-era `fileLib` declarations (`JJFB_PATH_MAX`, `my_resolve_path`) that are not present in the extracted base headers used for this offline check.

Comparison result:

- v55 `bridge.c`: same two pre-existing errors.
- v56 `bridge.c`: same two pre-existing errors.
- No new compiler errors were introduced by the v56 hook cases or address table.
- Python scripts pass `py_compile`.
- ZIP integrity test passes.

The authoritative build remains the supplied PowerShell/MSYS2 32-bit build on the user’s Windows project tree.
