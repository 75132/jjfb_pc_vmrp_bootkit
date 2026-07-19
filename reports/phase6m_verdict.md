# Phase 6M — verdict

## Bottom line

**Classification: `CFUNCTION_PUBLICATION_SOURCE_ZERO`**

P+0xC is not 'unwritten'; `dsm:cfunction.ext @ 0x94F04` **writes 0** across the 20-byte P (including +0xC). Host `_mr_c_function_new` also memset-zeros P (DOCUMENTED). `chunk_field_04=0` / `NONE_BEFORE_SELECT` means no published extChunk init_func (chunk+4), distinct from P+0xC.

## Facts

- CFN enter/disasm tags: `yes`
- 0x94F04 zero path: `True`
- natural P+0xC nonzero: `False`
- chunk_field_04 missing: `yes`
- extChunk magic candidates: `0`
- wxjwq same zero-writer (if run): `yes`

## Forbidden

Do not invent P+0xC, hardcode chunk, jump clusters, retry entry ABI matrix, or chase UI.

## Next phase (6N) unique direction

Restore/locate the **legitimate** `mrc_extChunk` allocation/register publication so P+0xC and chunk+4 become natural nonzero — only with DOCUMENTED/CROSS_TARGET contract evidence.

