# RA2 Toolbox 1.0.5

Windows 工具箱，集成高刷光标、单位统计覆盖层与战术助手。当前文件版本 **1.0.5.0**，Assist 协议 **44**；EXE 与 GameRuntime.dll 配套更新。

- [下载完整运行包 / EXE-DLL 更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.5)
- [展开源码与构建脚本](source/)、[本版更新说明](packages/v1.0.5/RELEASE_NOTES.md)
- [计算位置、地图与科技全开实现分析](source/RUNTIME_COMPUTE_AND_UNLOCK_REVIEW.md)
- [Hook 修改游戏判断清单](source/HOOK_JUDGMENT_CHANGES.md)
- [本版采样回归记录](packages/v1.0.5/ROUND_TRANSITION_VERIFICATION.json)、[PE32 构建记录](packages/v1.0.5/WINDOWS_BUILD_ARTIFACTS.json)、[架构检查及剩余项](packages/v1.0.5/ARCHITECTURE_AUDIT.json)
- [历史功能目录](docs/FEATURES.md)和[1.0.2 架构说明](docs/FULL-1.0.2-Architecture-and-Features.html)用于查阅；当前开放/停用状态以下文及源码为准。

## 本版修复

单位统计增加玩家身份、名单、败退后恢复和采样前后 fresh 核对；对局变化或不完整读取时清空实时数据和旧画面。自己阵亡后仍统计活着的对手。保留持久图标缓存。尚未在用户平台现场复现，修复针对代码中已确认的生命周期缺口。

14 个实际 Sampler 回归用例通过 ASan/UBSan，PE32/ABI/依赖/版本及架构检查通过。GameRuntime 可执行代码段与 1.0.4 相同。未进行 Windows 游戏、真实双机联机或实际延迟验证；模型测试不等同于这些验证。

## 当前状态

- 装车、维修、仅选中载具逃生等复用统一入口；部分业务计算仍在 Runtime。此次只分析取舍，尚未迁移。
- 智能卸载、科技全开、敌方建筑变卖仍停用；不要把目录或遗留代码当作可用入口。
- 地图全开需显式勾选实验许可，当前仍调用原版 Reveal；敌方位置标记为外部只读覆盖层，两者不同。
- 保留 1.0.4 的原生飞机路径点与任意建造实验开关：默认关闭，标注联机危险。所有三个允许的修改位置由同一 GameRuntime 管理，不代表已经证明联机安全。
- 外部读取器共用连接管理，各任务独立采样缓存；尚非所有功能共用一次原子的全世界快照。

## 安装与构建

完整包解压后运行 RA2Toolbox.exe；Update 包覆盖已有 1.0.2 或更新完整目录。升级前关闭工具并退出游戏，避免固定驻留的旧 DLL 继续运行。可以保留 Assist.ini、settings.ini 及 cache/unit-icons。

使用 Zig 0.13.0，运行 source/build-zig.sh 或 source/BUILD_ZIG.cmd。源码不包含游戏本体。

Release 附件仅含运行程序和所需资源，不放项目源码或测试材料；源码另存仓库 source/。GitHub 自动生成的 Source code 链接属于仓库快照。历史 payload/release、旧报告及开发包不是本版构建产物。
