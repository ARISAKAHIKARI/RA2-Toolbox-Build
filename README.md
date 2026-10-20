# RA2 Toolbox 1.0.25

Windows高刷光标、单位统计与战术助手。文件版本 **1.0.25.0**，Assist协议 **52**，Runtime诊断协议 **4**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.25)
- [源码](source/) · [本版更新](packages/v1.0.25/RELEASE_NOTES.md)
- [本次审查、原因判断和实现](source/REVIEW-1.0.25.md) · [验证证据](source/verification/review-125/)

修复回调Win32状态泄漏、减少重复检查、增加游戏线程异常与进程退出证据。保留统一管理器与原生事件通道，没有新增游戏代码补丁位置，未修改网络协议或同步检查。尚未证明战网断线已解决。

退出游戏和工具箱后成套更新EXE/DLL，保留配置和cache。使用Zig 0.13.0运行source/build-zig.sh或source/BUILD_ZIG.cmd。受控验证不等于真实Windows平台或双机联机验收；历史报告仅对应各自版本。
