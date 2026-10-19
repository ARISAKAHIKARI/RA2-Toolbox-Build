# RA2 Toolbox 1.0.21

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.21.0**，Assist 协议 **52**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.21)
- [源码](source/) · [本版更新](packages/v1.0.21/RELEASE_NOTES.md)
- [实现说明](source/REVIEW-1.0.21.md) · [验证记录](source/verification/review-121/)

三项玩家控制功能置灰停用。地图不渲染黑幕与迷雾、飞机原生路径点作为普通设置显示，保留原实现和配置。

退出游戏与工具后成套更新 EXE/DLL，保留配置和 cache。Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。

本轮有实际编译 DLL 的模拟测试和 UI 控件验证，没有真实 Windows/LBW 或双机联机验收。历史报告只对应各自版本。
