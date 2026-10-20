# RA2 Toolbox 1.0.26 正式版

Windows高刷光标、单位统计与战术助手。文件版本 **1.0.26.0**，Assist协议52，Runtime协议4。

- [正式版完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.26)
- [源码](source/) · [版本说明](packages/v1.0.26/RELEASE_NOTES.md)
- [本次审查](source/REVIEW-1.0.26.md) · [验证证据](source/verification/review-126/)

修复科技侧栏生命周期判断，补齐现有补丁ABI依赖验证和变化后的恢复。保持统一管理器与事件入口。GitHub Release不标记预发行，已有实验功能继续单独标注。

退出游戏和工具箱后成套更新EXE/DLL，保留配置和cache。使用Zig 0.13.0运行source/build-zig.sh或source/BUILD_ZIG.cmd。受控回归不等于真实平台双机联机验收；历史报告仅对应报告中标注的版本和哈希。
