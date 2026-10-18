# RA2 Toolbox 1.0.15 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.15.0**，Assist 协议 **49**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.15)
- [源码与构建脚本](source/) · [本版说明](packages/v1.0.15/RELEASE_NOTES.md)
- [修复与验证说明](source/REVIEW-1.0.15.md) · [验证记录](source/verification/review-115/)

修复原生侧栏预定时的拒绝语音、预定生产时其他图标不变灰；预定队列与候选按钮增加 10 px 间隔和分隔线。继续使用统一 GameRuntime，无新 Hook，仿真指令仍走原版构造与统一提交入口。

退出游戏与工具后成套更新 EXE/DLL，保留配置和 cache。使用 Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只含运行文件和说明；源码独立保存，GitHub 自带 Source code 链接是仓库快照。

本轮为编译、静态检查及夹具验证，没有 Windows/LBW 实机画面、声音或双机验收。历史报告仅代表对应版本。普通步兵原生追加路径爆破研究仍未完成；实验功能保留原有开关和标注。
