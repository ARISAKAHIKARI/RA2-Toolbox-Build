# RA2 Toolbox 1.0.10 Beta

Windows高刷光标、单位统计与战术助手。文件版本 **1.0.10.0**，Assist协议 **46**，EXE/DLL配套。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.10)
- [源码与构建脚本](source/) · [本版发布说明](packages/v1.0.10/RELEASE_NOTES.md)
- [实现和测试边界](source/REVIEW-1.0.10.md) · [验证汇总](source/REVIEW_VERIFICATION.json)
- [Hook判断修改清单](source/HOOK_JUDGMENT_CHANGES.md) · [科技/C4原版路径调查](source/NATIVE_TECH_C4_REVIEW.md)

本版修复覆盖层尺寸、预定工厂读取一致性与智能卸载入口/下车散开；加入默认关闭的科技生产和C4 Beta。C4按用户要求允许联机显式测试，但修改模拟校验可能造成不同步/掉线，不能因统一入口或Beta标签而视为安全。科技暂只开放兵/车/飞机，建筑外部放置未验证。

仍为一个GameRuntime注入DLL、宿主规划和统一原版事件构造。补丁全部由GameEntryManager管理；无功能私自Hook或注入。部分旧业务计算仍在Runtime，未宣称全迁出。

关闭游戏和工具后更新，保留配置/cache。Zig0.13.0构建source/build-zig.sh或source/BUILD_ZIG.cmd。Release附件仅运行文件，GitHub自动Source code链接是仓库快照。

测试为夹具与静态检查，未运行真实Windows/LBW游戏、像素截图或双机联机；历史报告不代表本版验证。
