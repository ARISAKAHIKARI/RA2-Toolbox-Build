# RA2 Toolbox 1.0.9

Windows 高刷光标、单位统计覆盖层与战术助手。文件版本 **1.0.9.0**，Assist 协议 **45**；EXE/DLL 配套更新。

- [完整运行包 / EXE-DLL 更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.9)
- [源码及构建脚本](source/)、[发布说明](packages/v1.0.9/RELEASE_NOTES.md)
- [本版实现与验证边界](source/REVIEW-1.0.9.md)、[验证汇总](source/REVIEW_VERIFICATION.json)
- [科技和 C4 原版路径调查](source/NATIVE_TECH_C4_REVIEW.md)、[Hook 修改清单](source/HOOK_JUDGMENT_CHANGES.md)

本版修复预定未落地建筑误取消与暂缺前置三次重试后清队列，支持原生侧栏点击预定及编号；补充栏只列预定和原生栏缺失的未来建筑。每类 5 项，右键取消依赖后缀。

保持单个 GameRuntime 注入、宿主排队和统一原版事件构造，无新增 hook。科技全开、普通步兵 C4 尚未实现；智能卸载和敌方建筑变卖仍停用。任意建造/原生飞机路径点仍默认关闭并提示联机危险。部分业务计算仍在 Runtime，未宣称全量迁出。

关闭游戏和工具后更新。Update 覆盖已有 1.0.2 或更新完整目录；Windows 包含完整资源；保留配置及 cache。Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。

Release 附件只含运行文件；GitHub 自动生成的 Source code 链接为仓库快照。测试为夹具、静态验证，未运行真实 Windows 游戏、LBW、双机联机或像素截图。历史报告不代表本版验证。
