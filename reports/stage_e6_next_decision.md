# Stage E6 — next decision

**branch:** `LIFECYCLE_EVENT_REQUIRED`

| Signal | Value |
|--------|-------|
| MRC_INIT | yes |
| ret0=0 | **yes** |
| 10113 wrote fp | yes |
| 10102/10120/10140 REGISTER | yes |
| 10162 ALLOC | yes |
| DRAW/REFRESH | no (60s window) |

E6 closed PLATFORM_RET0_CAUSE. Next: after successful init, why no timer arm / lifecycle / DRAW.
