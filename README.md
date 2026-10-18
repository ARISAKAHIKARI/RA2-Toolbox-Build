# RA2 Toolbox 1.0.8

Windows 高刷光标、单位统计覆盖层与战术助手。文件版本 **1.0.8.0**，Assist 协议 **45**；EXE 和 GameRuntime.dll 必须配套更新。

- [完整运行包 / EXE-DLL 更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.8)
- [源码及构建脚本](source/)、[本版发布说明](packages/v1.0.8/RELEASE_NOTES.md)
- [逃生根因、地图绘制测试、验证边界](source/ESCAPE_AND_RENDER_REVEAL.md)
- [Hook 修改清单](source/HOOK_JUDGMENT_CHANGES.md)、[计算位置与解锁方案审视](source/RUNTIME_COMPUTE_AND_UNLOCK_REVIEW.md)

## 本版

建造预定按实际游戏/窗口尺寸缩放，建筑与防御各 5 项编号队列、右键取消。没有电厂但正在造电厂时可预定灰色兵营，取消前置会取消后续链。停工提醒仅建筑/载具栏，读取原生 SHP 换色闪烁。

[本版实现与测试范围](source/REVIEW-1.0.8.md) · [验证汇总](source/REVIEW_VERIFICATION.json)。36 项 DLL/构造器模拟、宿主队列/前置与 SHP ASan/UBSan、6 种分辨率和 UI/架构检查通过；未运行真实 Windows 游戏、LBW、平台联机或像素截图。

## 当前状态

保留 1.0.5 单位统计跨局清理和持久图标缓存。智能卸载、科技全开、敌方建筑变卖仍停用；业务计算尚未迁移。任意建造与原生飞机路径点仍默认关闭、标记联机危险。外部读取器共用连接管理，各任务独立采样缓存，并非一次原子的全世界快照。

## 使用

升级前退出游戏、关闭工具，再替换 EXE 和 DLL。Update 包覆盖已有 1.0.2 或更新完整目录；Windows 包含完整运行资源。保留 Assist.ini、settings.ini 和 cache/unit-icons 即可。

用 Zig 0.13.0 运行 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只放运行程序及依赖资源；项目源码和验证记录另在仓库。GitHub 自动生成的 Source code 链接为仓库快照。历史文档和报告不代表本版验证结论。
