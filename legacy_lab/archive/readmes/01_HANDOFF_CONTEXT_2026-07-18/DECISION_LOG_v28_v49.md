# DECISION_LOG_v28_v49 摘要

- v28-v31：确认 `_strCom(601/800/801)`、loader 返回、sendAppEvent/timer/文本链路。
- v32-v34：追 11F00/blit/chrome/native present，发现不应把 chrome 完整性作为主目标。
- v35-v39：追第一屏、真实资源、bitmap object、slogo gate，确认 0x10134 必须返回 pixel ptr。
- v40-v42：发现 loadingbar 真实 draw 入口、240×320 轴错误、0xF81F 透明色问题。
- v43-v44：确认 forced 0x45 下 UI/状态机空转；AC8/progress/ui_mode 无自然写。
- v45-v48：尝试事件矩阵、progress driver、分支/event code；确认 progress 能让 bar 动但不能进入检查更新，说明推 jjfb 内部 UI 不是主路线。
- v49：方向纠偏。目标不是还原 jjfb UI/动画，而是仿冒泡网游/gwy 启动链，绕过 gwy 外壳强制更新，建立 GWY Launcher Shim。
