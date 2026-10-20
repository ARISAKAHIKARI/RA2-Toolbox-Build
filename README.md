# RA2 Toolbox 1.0.24

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.24.0**，Assist 协议 **52**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.24)
- [源码](source/) · [本版更新](packages/v1.0.24/RELEASE_NOTES.md)
- [实现、平台证据和限制](source/REVIEW-1.0.24.md) · [验证记录](source/verification/review-124/)

修正黑幕绘制遗漏资源初始化、科技补图开局集中处理和回调环境隔离，保留上一版飞机Z路径修正。未改变网络协议或同步检查，不假定任一平台启用了ProtocolZero。尚未证明战网断线已解决。

退出游戏与工具箱后成套更新EXE/DLL，保留配置和cache。使用Zig 0.13.0运行source/build-zig.sh或source/BUILD_ZIG.cmd。本轮为实际编译代码的受控验证，没有真实平台或双机验收；历史报告只对应各自版本。
