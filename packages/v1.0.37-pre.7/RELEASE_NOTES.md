# v1.0.37-pre.7

修复磁电/高级移动停止后，后续普通攻击只转炮塔、射程外不追击的问题。用户日志显示当前任务停留在 Sticky=6，而新 Attack 一直位于待处理队列；两份游戏 EXE 的原版任务就绪检查明确拒绝从 Sticky 切换。本版将规划器的停驻命令改为可中断的 Guard=5，并拒绝旧 Sticky 提案。

退出游戏与工具后同时替换 EXE、DLL，**开新对局验证**。已经卡住的旧对局或存档不会被本版自动修复；本版没有强写单位任务或修改原版引擎分支。

- 修正 SHP/VXL 磁盘回退中 ImageFile 起点 +1FC→+1F8，避免树名 TREE25 被读成25，以及其他图像文件名前四字符丢失。
- 地形不再因无关 flag 位、没有附加图时的未使用坐标而整块拒绝，补充具体格式与内存回退诊断。用户现场 SNO 地图显示仍待实机复测。
- 保留 pre.6 的队形右拖、增强对象攻击、SHP格式2和步兵预览朝向修正。

验证包含真实原版 QueueMission/ReadyToNextMission/NextMission、实际 DLL 内外计算和两种事件队列、攻击/停止/重获目标、两武器目标类型与范围、资源解码和观察窗缓存。旧 DLL 在新增 Guard 准入测试失败，修复版通过。历史 advanced_regression.cpp 的旧夹具在未改动的 pre.6 中也失败，单独记录，未计入通过结果。受控验证不代表完整 Windows 寻路、画面或双机联机验证。

[研究证据与未决项](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/codex/fire-formation-v137-pre1/source/research/2026-10-24-sticky-assets-137-pre7.md)
