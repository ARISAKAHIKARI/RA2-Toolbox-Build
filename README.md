# RA2 Toolbox 1.0.14 Beta

Windows 高刷光标、单位统计与战术助手。文件版本 **1.0.14.0**，Assist 协议 **48**。

- [完整运行包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.14)
- [源码与构建脚本](source/) · [本版说明](packages/v1.0.14/RELEASE_NOTES.md)
- [修复与验证说明](source/REVIEW-1.0.14.md) · [验证记录](source/verification/review-114/)

本版让预定面板随实际内容调整高度、半透明显示，并在成功点击预定时通过同一 GameRuntime 播放原生建造点击音效。继承 1.0.13 的图标缓存、预定生命周期、可配置索敌范围、修饰键与卸载修复。

关闭游戏与工具后成套更新 EXE/DLL，保留配置和 cache。使用 Zig 0.13.0 构建 source/build-zig.sh 或 source/BUILD_ZIG.cmd。Release 附件只含运行文件和说明；源码独立保存，GitHub 自带 Source code 链接是仓库快照。

本轮为编译、静态检查及夹具验证，没有 Windows/LBW 画面、声音或双机验收。历史报告仅代表对应版本。普通步兵原生追加路径爆破研究仍未完成；实验功能保留原有开关和标注。
