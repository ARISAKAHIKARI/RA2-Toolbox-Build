# 硬件断点：外部异常快速分流与真实命中率窗口

## 问题与证据范围

在 `cee2af5621c78d5f565463cf611ff591ed1ecbc3` 的第一轮去冗余之后，用户要求继续处理硬件控制卡顿。
本轮先查 research INDEX、控制单槽 ABI、既有断点热路径及 owner 交会笔记，并查询同一持久
`RA2-Reverse-Engineering-Notes.sqlite` v159。当前工作基线 `c03e4ffcba13c6ceb435b758989d7c95093ca772`
只在 pre.1 之上增加统一采样契约，硬件生产实现相同。

沿用既有支持样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
控制断点仍为 `0x50B6F0`；ECX 为目标 House，[ESP] 为原调用返回地址；严格返回位置
`0x6339BF` / `0x637E45` 仍转向 `0x50B716`，其他合格调用仍转 `0x50B710`。
本轮没有新游戏地址、反汇编、私人映像访问、原生游戏执行或 Windows DR 测量。

## 一、外部异常不应进入工具自己的清理框架

旧 `techbp::Service::vectored` 在确认 owner 线程后，对该线程的所有异常先递增 `handlerDepth_`，
进入 `RuntimeScopedCall`，再由 `dispatch` 发现它并非本方异常。这包括外部 AV/in-page、普通
breakpoint、非本方 DR 槽、混合 BD/BS/BT、错误 EIP/DR7 或没有 debug context 的 single-step。

新 `classify` 只读取传入 CONTEXT 与 owner 的 lease，返回 Ignore/PrivateTrap/Witness/Feature。
Ignore 原样转交，不创建本方 SEH 清理帧、不改深度/统计/上下文。实际私有 rendezvous、硬件
witness 与本方功能命中仍建立 `RuntimeScopedCall`，保持被外部处理者展开时的清理。
同一 CONTEXT 只分类一次；没有 witness lease 时跳过那份空 lease 的归属分类。

这是减少本工具在外部异常链里的冗余，不是减少控制入口的硬件命中次数。控制的世界/House
身份、stop/fault、即时模式、caller 检查均保留；没有跨调用缓存准入成功。
不认领 foreign/mixed 原因，不轮换或暂停 DR，也不改变四槽预算或自动回退代码补丁。

## 二、维护发布复用同一状态视图

`publishDebugBreakpointStates` 原先在每个 UI 行重复调用 `pendingFor`，未请求行还经 `groupOff`
再调用一次。现在普通 owner 发布先取得一次 pending groups、retained tags、uncertain 与 resources，
六行复用同一个本地视图。此函数不调用游戏、不进行 DR 事务；`debugentries::installation`
只修改观测表，不改变 Service 所有权。清理状态与错误呈现的判定保持原语义。

## 三、把已有计数变成可读取的时间窗口

此前 `controlExceptions` 已在 owner 回调无 LOCK 递增，却没有导出到 Shared，日志只能看到
controlStatus，无法知道每秒实际命中多少次。本轮新增固定 96 字节的 `hardwareperf::Snapshot`，
由 ordinary `publish` 在约一秒窗口、换 match 或帧回退时通过 seqlock 发布：

- `cumulative` 与 `delta`：总 owned 异常、所有功能成功改写、control/full/enhancement/map
  的原有回调计数，以及控制签名准入检查次数与世界捕获尝试次数。
- `tick` / `intervalMs`：这次真实观察窗口；首次或世界重置时 `intervalMs=0`，不能计算速率。
- `firstLogicFrame` / `lastLogicFrame` / `observedLogicFrames`：维护点看到的帧号，不是每次异常
  的帧归属或真实游戏 FPS。跳过的帧不会被虚构成观测。

Host 可计算 `delta.control * 1000.0 / intervalMs`，回答现场控制异常每秒的数量。
`changed` 包含整个硬件 Service 的成功功能回调，含单机 FPS，不是控制独有成功数；各功能命中
计数是分支尝试次数，不代表最终允许。`admissionChecks` 包含调试页的控制签名观察；
`worldCaptures` 是捕获尝试次数，不是成功采样帧数。计数与 TickCount 用无符号差值处理回绕。

每次硬件异常没有新增计时、日志、分配或 frame 读取，也没有新增统计递增；窗口复用原计数。
新增的两项维护计数只在普通准入/捕获调用中递增。协议字段与配对构建由本轮整合集成更新。

## 验证与未解决边界

本轮实际执行：

| 检查 | 结果与范围 |
| --- | --- |
| 完整生产 `Service`，受控 Win32 边界 | 31 场景、1461 检查通过，包括 16 种 foreign/native/mixed 快速分流与原 witness、事务、线程身份、嵌套、展开、撤回路径 |
| 固定 pre.1 `Service` 负对照 | 在首个新增“foreign 不构建 scope/不碰深度”断言失败；未把负对照计作通过 |
| 实际控制/Full 回调片段 | 2752 检查通过；严格 caller、仅改 EIP/对应 bool、即时拒绝与原始字节条件仍保留 |
| 生产 1 秒窗口 reducer | 2027 检查通过；同帧重复维护、窗口门控、TickCount/计数回绕、match 变化、rewind、暂停和跳帧；固定布局 96 字节 |

复现命令：

```sh
python3 source/assist/tests/breakpoint_owner_rendezvous.py --out /tmp/hardware-owner
python3 source/assist/tests/control_parameters.py --out /tmp/hardware-control
g++ -std=c++17 -O2 -UNDEBUG source/assist/tests/hardware_performance.cpp -o /tmp/hardware-performance
/tmp/hardware-performance
```

负对照给第一条追加 `--core <fixed-pre.1>/source/assist/source/tech_ui_breakpoint_core.h`。
源摘要与收据摘要见相邻 evidence JSON；整合编译与最终配对产物结果另行记录。

已证明 foreign 路径不再执行该层工具 bookkeeping；没有本轮 Windows 整条 DR/VEH 延迟或游戏
FPS 数据。原公共控制入口每次命中仍有 CPU 异常、Windows 派发/恢复成本。既有原生证据不支持
一个能保留全部 caller 语义、保持单槽且明显减少命中的替代地址；未经证据核定不后移或缩小
拦截范围。此修改让现场开销可量化，并不宣称已根治用户的全部卡顿。
