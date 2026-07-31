@echo off
set GWY_LAUNCH=1
set GWY_LAUNCH_TARGET=gwy/jjfb.mrp
set GWY_LAUNCH_PARAM=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
set GWY_RESOURCE_ROOT=c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\320x480
set GWY_OVERLAY_ROOT=c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay
cd /d "c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run"
main.exe > "c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\probe_cmd_out.txt" 2> "c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\probe_cmd_err.txt"
