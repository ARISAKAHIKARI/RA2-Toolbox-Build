# RA2 Toolbox 1.0.23

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.23.0**，Assist 协议 **52**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.23)
- [源码](source/) · [本版更新](packages/v1.0.23/RELEASE_NOTES.md)
- [实现与审查范围](source/REVIEW-1.0.23.md) · [验证记录](source/verification/review-123/)

修复联机飞机路径记录被 Z 规划状态误拦截，补充失败提示和日志。未重做绘制未揭示区域、科技全开、任意建造；这些功能的战网掉线仍待定位。

退出游戏与工具后成套更新 EXE/DLL，保留配置和 cache。Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。本轮为真实编译代码的受控模拟验证，没有真实平台或双机验收；历史报告只对应各自版本。
