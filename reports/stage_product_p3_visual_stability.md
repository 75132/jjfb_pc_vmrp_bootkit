# Stage Product P3 Visual Stability

- **run_id:** p3_validate_20260723_015539_39363
- **mode:** validate
- **verdict:** NATURAL_CALLBACK_STABLE_NO_NEW_DRAW
- **primary_fault_class:** CALLBACK_RETURN_SENTINEL_OK
- **timer_cadence:** TIMER_CADENCE_VALID
- **runtime:** Gwy+stubs
- **seconds:** 45
- **process_exit:** killed
- **ok_callback_returns:** 62
- **SCHEDULER_NATURAL_CALLBACK:** yes
- **ROBOTOL_INIT_RETURN_ZERO:** yes
- **draw_seen:** no
- **refresh_seen:** no
- **framebuffer_nonempty:** no
- **hwnd_visible:** no
- **frame_captured:** no
- **callback_frame_milestones:** yes
- **forbidden_hits:** none
- **manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_p3_manifest_p3_validate_20260723_015539_39363.txt
- **evidence_png:** none

## Exact marker gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | yes |
| NO real guest fault | yes |
| CALLBACK_RETURN_SENTINEL_OK / ok returns | yes (62) |
| FIRST_NATURAL_DRAW / JJFB_DRAW | no |
| FIRST_NATURAL_REFRESH / JJFB_REFRESH api=_DispUpEx | no |
| FRAMEBUFFER_NONEMPTY | no |
| HWND_VISIBLE | no |
| FIRST_NATURAL_FRAME_CAPTURED | no

## Notes

- stop_at_base + uc_err=0 is CALLBACK_RETURN_SENTINEL_OK, not a guest fault
- BIND_REFRESH is not counted as DRAW/REFRESH
- Strong success: PRODUCT_FIRST_NATURAL_FRAME_STABLE
- Valid intermediate: NATURAL_CALLBACK_STABLE_NO_NEW_DRAW
