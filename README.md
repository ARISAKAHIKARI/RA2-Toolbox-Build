# RA2 Toolbox 1.0.16 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.16.0**，Assist 协议 **50**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.16)
- [源码与构建脚本](source/) · [本版说明](packages/v1.0.16/RELEASE_NOTES.md)
- [架构、位置范围及验证说明](source/REVIEW-1.0.16.md) · [验证记录](source/verification/review-116/)

修复新进程/正式窗口绑定与类型目录迟到加载；加入小地图画面放大和八项真实双计算后端。事件仍经统一 GameRuntime，未增加独立 Hook。并非所有功能都已有双路径，固定位置清单见说明。

退出游戏与工具后成套更新 EXE/DLL，保留配置和 cache。Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件仅运行文件及说明，源码独立保存。

本轮为编译、源审计及模拟夹具验证，无真实 Windows/LBW 捕获、双机联机或延迟验收。历史报告只代表相应版本。
