# RA2 Toolbox 1.0.11 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.11.0**，Assist 协议 **46**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.11)
- [源码与构建脚本](source/) · [本版发布说明](packages/v1.0.11/RELEASE_NOTES.md)
- [缩放实现和验证边界](source/REVIEW-1.0.11.md) · [验证记录](source/verification/review-111/)
- [原生路径追加研究](source/NATIVE_PLANNING_APPEND_REVIEW.md)

新增三类覆盖层独立缩放，50–250%，默认 100%，立即生效并自动保存。保留分辨率适配，预定面板显示与点击共同缩放。本版无新增游戏 Hook，注入 DLL 的 .text 与 1.0.10 一致。

原生路径追加研究仅验证到合成世界中的复制和分派；尚未交付普通步兵通过该路径爆破、占领或潜入的功能。此前实验功能仍有各自限制与风险。

关闭游戏与工具后更新 EXE / DLL，保留配置和 cache。使用 Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只含运行文件和说明，源码独立保存在仓库；GitHub 自动生成的 Source code 链接是仓库快照。

当前验证为编译、静态核对和夹具，未运行真实 Windows 游戏、LBW、像素截图或双机联机。历史报告仅代表其对应版本。
