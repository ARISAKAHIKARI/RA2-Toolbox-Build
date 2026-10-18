# RA2 Toolbox 1.0.7

Windows 高刷光标、单位统计覆盖层与战术助手。文件版本 **1.0.7.0**，Assist 协议 **45**；EXE 和 GameRuntime.dll 必须配套更新。

- [完整运行包 / EXE-DLL 更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.7)
- [源码及构建脚本](source/)、[本版发布说明](packages/v1.0.7/RELEASE_NOTES.md)
- [逃生根因、地图绘制测试、验证边界](source/ESCAPE_AND_RENDER_REVEAL.md)
- [Hook 修改清单](source/HOOK_JUDGMENT_CHANGES.md)、[计算位置与解锁方案审视](source/RUNTIME_COMPUTE_AND_UNLOCK_REVIEW.md)

## 本版

整体检查后修复满队列漏观察逃生部署、实验回调缺少菜单检查、停止后未请求遮罩重绘、科技目录/风险说明控件重叠及截图诊断协议过时。没有增加 Hook 位置或私有入口。

[整体审视与测试范围](source/REVIEW-1.0.7.md) · [最终构建验证汇总](source/REVIEW_VERIFICATION.json)。实际 DLL 模拟、双端按帧模型及 UI/算法回归通过；未运行真实 Windows 游戏、LBW、平台联机或像素截图。

## 当前状态

保留 1.0.5 单位统计跨局清理和持久图标缓存。智能卸载、科技全开、敌方建筑变卖仍停用；业务计算尚未迁移。任意建造与原生飞机路径点仍默认关闭、标记联机危险。外部读取器共用连接管理，各任务独立采样缓存，并非一次原子的全世界快照。

## 使用

升级前退出游戏、关闭工具，再替换 EXE 和 DLL。Update 包覆盖已有 1.0.2 或更新完整目录；Windows 包含完整运行资源。保留 Assist.ini、settings.ini 和 cache/unit-icons 即可。

用 Zig 0.13.0 运行 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只放运行程序及依赖资源；项目源码和验证记录另在仓库。GitHub 自动生成的 Source code 链接为仓库快照。历史文档和报告不代表本版验证结论。
