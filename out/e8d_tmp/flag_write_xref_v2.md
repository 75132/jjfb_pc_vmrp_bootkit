# E8D flag write xref v2
ext=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext
code_base=0x2D8DF4

Detects direct STRB/STRH/STR via literal+R9, aligned wider stores covering the byte,
and nearby-band BL candidates (memcpy-like heuristic).

## offset 0xC44 (site 0x3066AC)
- direct_literal_stores: 9
- wider_aligned_cover: 18
- memcpy_like: 0
  - str store=0x2E87F2 via=literal_offset class=bl_0x2D976A
  - strb_reg store=0x2F4E82 via=literal_offset class=robotol_internal
  - str store=0x2F7F56 via=literal_offset class=unknown
  - strb_reg store=0x2FB286 via=literal_offset class=robotol_internal
  - str store=0x2FC8CA via=literal_offset class=robotol_internal
  - strb_imm store=0x2FEDFA via=literal_offset class=robotol_internal
  - strb_reg store=0x2FEE4E via=literal_offset class=robotol_internal
  - strb_reg store=0x30CC72 via=literal_offset class=unknown
  - strb_reg store=0x311C3E via=literal_offset class=robotol_internal
  - str store=0x2E87F2 via=str_covers_byte class=bl_0x2D976A
  - strb_reg store=0x2F4E82 via=str_covers_byte class=robotol_internal
  - str store=0x2F7F56 via=str_covers_byte class=unknown
  - strb_reg store=0x2FB286 via=str_covers_byte class=robotol_internal
  - str store=0x2FC8CA via=str_covers_byte class=robotol_internal
  - strb_imm store=0x2FEDFA via=str_covers_byte class=robotol_internal
  - strb_reg store=0x2FEE4E via=str_covers_byte class=robotol_internal
  - strb_reg store=0x30CC72 via=str_covers_byte class=unknown
  - strb_reg store=0x311C3E via=str_covers_byte class=robotol_internal
  - str store=0x2E87F2 via=strh_covers_byte class=bl_0x2D976A
  - strb_reg store=0x2F4E82 via=strh_covers_byte class=robotol_internal
  - str store=0x2F7F56 via=strh_covers_byte class=unknown
  - strb_reg store=0x2FB286 via=strh_covers_byte class=robotol_internal
  - str store=0x2FC8CA via=strh_covers_byte class=robotol_internal
  - strb_imm store=0x2FEDFA via=strh_covers_byte class=robotol_internal
  - strb_reg store=0x2FEE4E via=strh_covers_byte class=robotol_internal
  - strb_reg store=0x30CC72 via=strh_covers_byte class=unknown
  - strb_reg store=0x311C3E via=strh_covers_byte class=robotol_internal

## offset 0xC9D (site 0x3066BC)
- direct_literal_stores: 0
- wider_aligned_cover: 14
- memcpy_like: 40
  - strb_reg store=0x2E3A68 via=str_covers_byte class=robotol_internal
  - strb_imm store=0x2F097A via=str_covers_byte class=robotol_internal
  - str store=0x2F7772 via=str_covers_byte class=unknown
  - strb_reg store=0x2FB008 via=str_covers_byte class=robotol_internal
  - strb_reg store=0x307796 via=str_covers_byte class=robotol_internal
  - str store=0x30AA42 via=str_covers_byte class=robotol_internal
  - str store=0x3115B4 via=str_covers_byte class=robotol_internal
  - strb_reg store=0x2E3A68 via=strh_covers_byte class=robotol_internal
  - strb_imm store=0x2F097A via=strh_covers_byte class=robotol_internal
  - str store=0x2F7772 via=strh_covers_byte class=unknown
  - strb_reg store=0x2FB008 via=strh_covers_byte class=robotol_internal
  - strb_reg store=0x307796 via=strh_covers_byte class=robotol_internal
  - str store=0x30AA42 via=strh_covers_byte class=robotol_internal
  - str store=0x3115B4 via=strh_covers_byte class=robotol_internal
  - memcpy_candidate_bl store=0x2D9B68 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2D9CE6 via=nearby_band_bl class=bl_0x2D8EDC
  - memcpy_candidate_bl store=0x2D9DCA via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA026 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA3A8 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA5E8 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA7C4 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA836 via=nearby_band_bl class=bl_0x2D99AC
  - memcpy_candidate_bl store=0x2DA95C via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DA9F6 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DAB4E via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DB922 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DB9B4 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DB9DC via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DBA40 via=nearby_band_bl class=robotol_internal
  - memcpy_candidate_bl store=0x2DBA82 via=nearby_band_bl class=bl_0x2D9648

### C9D special
- covered by wider STR/STRH of aligned base — not pure STRB miss

## offset 0xCD1 (site 0x3066C8)
- direct_literal_stores: 1
- wider_aligned_cover: 2
- memcpy_like: 0
  - str store=0x2F76D8 via=literal_offset class=unknown
  - str store=0x2F7292 via=str_covers_byte class=unknown
  - str store=0x2F7292 via=strh_covers_byte class=unknown

## offset 0xCF5 (site 0x3066D2)
- direct_literal_stores: 2
- wider_aligned_cover: 0
- memcpy_like: 0
  - str store=0x2E7DBC via=literal_offset class=robotol_internal
  - str store=0x2F7F2C via=literal_offset class=unknown

## offset 0x11B0 (site 0x3066DC)
- direct_literal_stores: 1
- wider_aligned_cover: 2
- memcpy_like: 0
  - str store=0x2F72D8 via=literal_offset class=unknown
  - str store=0x2F72D8 via=str_covers_byte class=unknown
  - str store=0x2F72D8 via=strh_covers_byte class=unknown

## C9D provenance summary
- wider store cover present
