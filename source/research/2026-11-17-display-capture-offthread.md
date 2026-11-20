# 显示采样离开游戏线程：零页世界边界 + Host VisualCache

> **历史记录状态（2026-11-20）：** 本文记录过渡期 Host VisualCache 实现。后续 PR #67 将独立 Host 全图采集与 HostDisplayOnly 零页边界从生产链路暂挂，待显示字段迁入统一 Runtime 快照后再恢复。

## 问题、基线与范围
本轮以 main 的 `3a6b7e8656a1b89bc4fee93d6de9c330c19011ef` 为基线，参考另一个尚未合并且包含大量独立功能更新的 PR #42 中已验证的显示订阅分流机制；只抽取最小可审核变更，没有合并它的预发布流程、其他功能修复或实验性后台命令采样。未获取、重新反汇编任何私有 EXE。本轮未改变游戏 ABI、字段偏移、Key/Page/Channel wire 布局；既有已知可执行文件 SHA-256 见研究 INDEX 中 2026-11-08 统一采样文档。

此前游戏端 `CaptureGraph` 即便仅对 Host 的单位气泡、容量图标、地图渲染提供显示数据，也会采集 Techno/House/Building 全图；Runtime 的 `Capacity` 标志还会使游戏线程遍历乘员；同一回调还会整理 Host 页面请求。默认 Host-first 纯计算不等于免除这些主线程操作。

## 已实现拆分
- 独立 `HostVisualPassengers/HostVisualCombat/HostVisualOre` 是只读显示订阅标签。仅 `Visual/Resources` 加 Host tags 才是纯显示；混合 `Visual|Combat` 和 `Resources|Passengers` 是命令订阅，fail-closed。
- 仅所有活跃读者都来自纯显示、`worldDemand == Base|HostDisplayOnly` 且 Runtime 的所有本地命令/自动任务/回执/原生维护门都为空时，`captureWorldFrame` 才发布零页 `Key` 世界身份。必须保留游戏帧、场景、House、地图尺寸和订阅连接的原始准入/前后独立核验。此分支 **不进入 CaptureGraph、不获取或排序主线程页请求、不消耗后续命令的页租约**。
- Host `VisualCache` 使用同一个 `Key` 绑定显示图，前后两次各读取世界/House/帧/边界尺寸，不再要求零页底图内含地图大小字段；Host 原生 `ReadProcessMemory` 读取需求字段，结果只允许显示，不能作为 EventClass 原生命令依据。
- 命令读者在 `ProcessReader::beginWorldFrame` 显式拒绝零页底图；Runtime `SampleReadScope` 也拒绝。相同逻辑帧中后续热键、任务或控制请求必须强制升级为一份新 serial 的完整游戏线程封口样本。完整采集、联机最终 UID/House/ManualToken 校验与原生调用不迁移。
- 生命周期重连、世界变更及 `retireWorldFrame` 都清理 boundaryOnly 标志。显示源身份和时间不被 Host 任意延长。

## 验证层级和未决事项
`world_frame_core.cpp` 真实生产 `Server/Client/Runtime/VisualCache` 的受控 Win32 接口实验，检查零页生产调用 0 page reads/0 bytes，Host 得到 Techno 类型和武器，混合订阅 fail-closed、同帧真实命令升级；`display_capture_offthread.cpp` 用 100k 请求证明显示路径不扫描/复制需求表且完整路径保留页租约；CI 执行 WorldFrame 核心、retention、战术采样以及配套 Windows Host/Runtime 编译/ABI。测试是否通过以 Actions 结果为准，不能把受控调用计数折算实际游戏 FPS。

仍在主线程：活跃自动/联机命令所需的完整权威 `CaptureGraph`、命令执行前实时成员/UID/House/mission 核查、原生队列/事件调用、部分类型目录/技术维护和必要世界身份采样。这是安全分阶段迁移：不能简单在另一线程抓到“差不多同一帧”就允许发联机命令，否则可能有对象复用、跨帧撕裂和失效任务。
