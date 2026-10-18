# RA2 Toolbox 1.0.13 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.13.0**，Assist 协议 **47**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.13)
- [源码与构建脚本](source/) · [本版说明](packages/v1.0.13/RELEASE_NOTES.md)
- [修复与验证说明](source/REVIEW-1.0.13.md) · [验证记录](source/verification/review-113/)

修复共享图标跨局缓存、预定队列的叠放/取消/生命周期及跨帧生产；新增默认＋2格的可配置索敌范围，支持单独修饰键，改进卸载后工程师寻址。继承 1.0.12 设施进驻修复和独立覆盖层缩放。继续使用统一 GameRuntime 和原版指令构造，没有新增 Hook。

关闭游戏与工具后成套更新 EXE/DLL，保留配置和 cache。使用 Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只含运行文件和说明；源码独立保存，GitHub 自带 Source code 链接是仓库快照。

本轮为编译、静态检查及夹具验证，没有真实 Windows/LBW/双机验收。历史报告只代表对应版本。普通步兵原生追加路径爆破研究仍未完成；实验功能保留原有开关和标注。
