# RA2 Toolbox 1.0.6

Windows 高刷光标、单位统计覆盖层与战术助手。文件版本 **1.0.6.0**，Assist 协议 **45**；EXE 和 GameRuntime.dll 必须配套更新。

- [完整运行包 / EXE-DLL 更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.6)
- [源码及构建脚本](source/)、[本版发布说明](packages/v1.0.6/RELEASE_NOTES.md)
- [逃生根因、地图绘制测试、验证边界](source/ESCAPE_AND_RENDER_REVEAL.md)
- [Hook 修改清单](source/HOOK_JUDGMENT_CHANGES.md)、[计算位置与解锁方案审视](source/RUNTIME_COMPUTE_AND_UNLOCK_REVIEW.md)

## 本版

逃生不再把部署动画当作完成。原版散开移动打断后，乘员独立等自己的移动稳定并重试；首次尝试不等待其他乘员，已排队/执行中的部署不重复发送。仍仅跟踪选中载具的乘员。

新增默认关闭的“地图全开：仅去黑幕 / 迷雾（绘制测试）”。只跳过两个遮罩绘制叶函数，不调用旧 Reveal；原版视野计算及目标判断保留。与旧地图全开许可互斥；建议新对局测试，隐藏对象可配合已有敌方标记。两个位置统一由 GameRuntime 安装、恢复、检查冲突。

验证执行实际 DLL、原版事件构造器和入口桩，但使用合成游戏状态及 Win32/部分函数模拟；未运行真实 Windows 游戏、截图、双机联机和延迟测试，不宣称联机安全。

## 当前状态

保留 1.0.5 单位统计跨局清理和持久图标缓存。智能卸载、科技全开、敌方建筑变卖仍停用；业务计算尚未迁移。任意建造与原生飞机路径点仍默认关闭、标记联机危险。外部读取器共用连接管理，各任务独立采样缓存，并非一次原子的全世界快照。

## 使用

升级前退出游戏、关闭工具，再替换 EXE 和 DLL。Update 包覆盖已有 1.0.2 或更新完整目录；Windows 包含完整运行资源。保留 Assist.ini、settings.ini 和 cache/unit-icons 即可。

用 Zig 0.13.0 运行 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只放运行程序及依赖资源；项目源码和验证记录另在仓库。GitHub 自动生成的 Source code 链接为仓库快照。历史文档和报告不代表本版验证结论。
