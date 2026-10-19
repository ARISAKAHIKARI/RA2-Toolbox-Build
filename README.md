# RA2 Toolbox 1.0.20 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.20.0**，Assist 协议 **52**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.20)
- [源码](source/) · [本版更新](packages/v1.0.20/RELEASE_NOTES.md)
- [科技与玩家控制实现说明](source/REVIEW-1.0.20.md) · [验证记录](source/verification/review-120/)

本版恢复科技解锁的原生侧栏路径，增加跨玩家手动操控、切换操控方和出售其他玩家建筑测试。玩家测试默认关闭，排除中立/平民/治安/观战方；所有指令仍由统一 Runtime 管理。

退出游戏与工具后成套更新 EXE/DLL，保留配置和 cache。Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。

本轮有真实编译 DLL 的模拟测试和原生流程局部验证，没有真实 Windows 购买、LBW 或双机联机验收。历史报告只对应各自版本。
