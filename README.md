# RA2 Toolbox 1.0.12 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.12.0**，Assist 协议 **46**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.12)
- [源码与构建脚本](source/) · [本版说明](packages/v1.0.12/RELEASE_NOTES.md)
- [碉堡、维修厂和船厂修复与验证](source/REVIEW-1.0.12.md) · [验证记录](source/verification/review-112/)
- [原生路径追加研究](source/NATIVE_PLANNING_APPEND_REVIEW.md)

修复高级进驻的设施容量和占用判定、在途维修预留及入堡回执。仍通过统一 GameRuntime 入口与原版合法性检查。保留独立覆盖层缩放及现有配置。

关闭游戏与工具后成套更新 EXE/DLL，保留配置和 cache。使用 Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只含运行文件和说明；源码在仓库独立保存。GitHub 自带 Source code 链接是仓库快照。

本轮验证为编译、静态检查及夹具，没有真实 Windows/LBW/双机验收。普通步兵原生追加路径爆破仍是未完成的研究；实验功能保留原有开关和标注。历史报告仅代表对应版本。
