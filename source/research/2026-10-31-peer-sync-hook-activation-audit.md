# 首次远端摘要匹配、Hook 生命周期与联机副作用复核

## 问题与证据边界

基于 `codex/crc-map-shadow-hotfix32-20261031` / `9af98529e3eac95766e5faa0f2c04c9d409a3dd3`。
用户确认是游戏同步错误，并补充：预先开启功能进入战网后掉线；关闭进局后再开没有效果；重启工具箱后又掉线。具体功能组合和双方 SYNC 尚未取得，不能把三种现象全部归为一个已确诊原因。

先检索 research/INDEX、HF32、HF31、控制回归、普通版不同步及 Hook 冲突笔记。当前工具没有提供按 Library 身份下载 `RA2-Reverse-Engineering-Notes.sqlite` 的接口，未读取或新建同名数据库；新发现保留在本笔记，供原文件后续写回。

这是源代码审查、新增策略/Windows 合成回归及生产编译工作，不是完整游戏或双机认证。沿用已有原生研究样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的地址约定；本轮没有执行该私人样本，不扩张支持范围，不发布游戏文件或转储。

## 新增激活门槛

旧门槛只有 House、运行标志、会话、帧号与可见战术视图，不包含远端同步证据。新路径不增加接收/CRC detour，而是读取原生 DoList：

1. 在当前对局/Scenario/House 数组/本机 House ID 身份内，先观察其他人类 House 的未执行 FrameInfo。
2. 同一个物理槽中的同一条 FrameInfo 随后被原生标为执行过，且游戏已越过该事件帧。
3. `(Frame-Delay)` 必须晚于本次绑定帧、位于仍有效的 256 帧历史窗口，远端 CRC 与本机原生 `B04474` 对应槽相同。
4. 当前活跃远端人类 House 各有一次证据，且原生 OutOfSync 未置位，才进入功能维护。单人多人模式仅在原生 MPlayerCount 明确为 1 时免除远端条件；未知人数不猜测为单人。

已执行的旧槽、只有帧号增加、固定秒数、未执行的收到封包都不能解锁。换身份、回退和重新连接需要新证据。每次最多扫描 128 条待执行记录，只读游戏内存，不调用 CRC，不推进 RNG，不发送/修改任何事件、摘要历史或平台验证。

宿主 `main.cpp` 在战术连接的 Runtime 注入/Windows message hook 安装之前先以 ReadSession 完成同样的只读观察；进入 Runtime 后 `bridge.cpp` 再独立取得新证据，且在科技字段修改、补丁维护、命令消费和自动功能之前等待。GameEntryManager 的 Install 分支再次核对本局批准身份，Restore 不受此限制；可选 `55DE81` 帧通知同样延后，等待不能被记成签名不支持。独立游标服务仍使用共享消息入口；不把它声称为模拟修改，也不宣称所有 Windows DLL 加载副作用均已排除。

地址/布局依据：YRpp `d979554` 的 EventClass（111 字节、执行位 +1、House +2、Frame +3、CRC +7、Delay +13）、SessionClass（MPlayerCount +314，即 A8B54C）、Unsorted::Network（OutOfSync=A8B8C2）。这些是公开头文件/已有原生研究依据；本轮没有在实际平台重跑 FrameInfo 的完整消费流程。若某平台不保留可观察的执行标记，状态会持续等待，不用超时强行放行。

## 已修的生命周期问题

* 启动签名原先在可操作/远端同步之前检查，并将第一次失败永久缓存。现在同步门槛之后仍检查全部原有精确签名；失败每秒重试，成功才缓存，换局/断开重置。没有接受未知字节或放宽签名。
* `gameinput::clear` 原先不接受 ERROR_INVALID_HOOK_HANDLE，而 x86 broker 已接受。Windows 已移除的 HHOOK 会使宿主清理一直失败，条件满足时遗留进程槽。改为幂等清理，并区分 target-slots 与 subscription-slots。8 是并行游戏进程槽，32 是功能订阅槽；同一 PID 的多个功能共用一个进程槽，均不等于游戏玩家数或可开功能数。
* Runtime `connect` 原先调用 cleanup 后，不检查 Assist 是否因未发外方事件等原因延后卸载，就撤销旧控制通道。现在必须确认 Assist 和 Cursor 均真正脱离，才能交接到新控制器。此路径与“重启工具箱”有关，但没有现场证据证明就是用户这次的根因。
* 控制组原先把 riskControlActive 设 false，已残留的裸 thunk 仍可能返回强制控制。现在三个入口在停用/部分失败时重放捕获的原生路径；已知原始 JMP 变体保留原目标。不会覆写外部冲突字节。
* Registry 增加 active 写入范围相交检查；现有源站点没有发现重叠，不宣称已发生过重叠事故。
* sender 的 fault 原先没有清零路径，会在 pinned DLL 中跨局保留。现在只在新 epoch、旧组安全卸载、编码缓存清理且无外方待发事件后解除；同局故障仍锁住，不自动重发。

状态沿用现有 commandStatus 字段：1 就绪、2 等待首次远端匹配、3 精确签名暂不兼容、4 已观察到摘要不匹配。Host 显示原因并记录人数/队列及签名字节差异；IPC 结构及 Assist83/Runtime4/entry3 不变。新代码需要新游戏进程加载；只重启工具箱不会替换游戏中已 pin 的旧 Runtime。

## 我们有哪些 Hook / patch

下表列的是管理器允许的 51 个原生站点，不代表当前都安装。完整机器清单由 `assist/tests/hook_inventory.cpp` 生成；它另列起始地址仍登记但安装已禁止的 14 个旧站点，并验证允许站点互不重叠。

| 组 | 个数 | 地址 | 实际作用与联机边界 |
| --- | ---: | --- | --- |
| 远端控制判定 | 3 | 50B6F0, 6339BA, 637E40 | 共用判定及 owner 查询；**不是只影响 UI**，单端开启的完整模拟一致性未证实 |
| 命令所属方 | 4 | 646EF5, 646FD2, 6FFC93, 6FFE2B | 改原生命令 House；需与发送策略配合，不能称为仅改变选中状态 |
| 原生网络发送 | 7 | 649CA0, 649E1E, 64B660, 64B9D4, 64B4EA, 649D61, 649DBC | 外方 House 保留、连续分包及编码缓存；跨发端同 House 排序和接收字典碰撞仍未解决 |
| 出售入口 | 2 | 6929E5, 4AC179 | 光标/出售所属方入口；最终事件与接收效果另算 |
| MegaMission 仲裁 | 1 | 646F4E | 原生成功入 OutList 后的工具仲裁；没有证据证明正常路径直接改世界，也未获整局认证 |
| 随处建造 | 3 | 4A8EB0；直接模式 4A8EC3, 4A918E | 本地下单/邻近性入口；实际放置走原生事件，不能据此保证所有规则/MOD 一致 |
| 科技侧栏 | 7 | 6A97D2, 6AA781；44574A, 445795, 4457D0, 445810, 6AA788 | 本地 CanBuild 调用参数/图标枚举保留；没有覆盖整个 4F7870 House::CanBuild |
| 飞机原生规划 | 2 | 7E2908, 416AB6 | UI 准入与到达推进；完整联机实验会本机推进规划/DoList，存在明确模拟一致性风险 |
| 黑幕/迷雾绘图 | 2 | 47F038, 47F274 | 保留资源初始化，再跳遮罩/写 alpha；与原生 Reveal 是不同路径，不能把整个绘图链认证为无间接副作用 |
| 直接全图 | 5 | 508E60, 508EBA, 508F8F, 577B3A, 6D88FC | 可见性、雷达和 SpySat 判断；可能到达真实 Reveal，不是纯像素修改 |
| MapIsClear 镜像 | 8 | 64DCCA, 4F576B, 4FC328, 577ACA, 577BC7, 577CA7, 577DB0, 577F48 | 一个 CRC 输入与自然写入跟踪；不覆盖全部 Cell/Visionary/RNG 后继 |
| 低伤害过滤 | 1 | 6F7D44 | 自动索敌阈值；联机子项开启会在本机影响各 House 的原生索敌判断，存在模拟分叉风险 |
| 相机/滚屏 | 2 | 6D8640, 4A9840 | 小地图边界钳制与滚动；高刷滚屏产品开关目前禁用，未知平台替换拒绝覆盖 |
| 建造时间提示 | 1 | 7F74D8 | 本地 tooltip vtable，先调用原生再查询/显示文字 |
| 帧完成通知 | 1 | 55DE81 | 重放原始帧计数写入后发只读通知，不运行第二套模拟调度 |
| C4 旧能力覆盖 | 2 | 51A4D8, 51C32B | 管理器认识地址，但 placementTick 明确只恢复、不安装；不是现有强制进入功能的活动路径 |

另有每个游戏共享的一对 Windows message hooks、诊断用 VEH，以及游标显示 API；它们与上述 51 个游戏地址、8 个进程槽是不同概念。旧 CS 646E90、NativeEvent 6FFE00、FootClick 4D74E0、Decide 692610、MouseUp 4AB9B0、Cursor 731BF0、旧帧入口 55D360、旧放置 464AC0 与六个旧 frame callsites 等仍有登记，但 manager size=0，当前不能安装。

## 不属于 Hook 但仍需审查的游戏写入

后续补充已在 [直接写入/清理遗漏审计](2026-11-01-latent-desync-mutation-audit.md) 修正科技加强联机直接写入、失败还原丢失记录、CRC 世界读失败时提前卸镜像，以及部分功能未还原仍确认 stop 的路径。下文风险描述保留原审查范围；不把首次同步门槛视作这些问题的总修复。

原生全图通过直接调用 577D90 修改 House+240/+241 和 Cells；科技加强直接写 21 种 Type+634；材质方案写 SHP/alpha 数据；原生规划推进可能写本机 DoList。统计/渲染/自动命令“使用原生接口”不自动等于没有单端副作用。

HF32 的 21 项 CRC 测试刻意在 577DB6 跳到 577F1E，未执行完整 Cell/Visionary 后半段。本次没有扩张这些测试的结论，也没有添加伪造 CRC、关闭比较或平台校验绕过。延后安装只能隔离开局时序，不能修复同 House 排序、压缩字典或真实模拟状态分叉。

## 验证入口与仍待完成的证据

* `initial_sync_gate.cpp`：生产只读策略、旧/本机/未执行/过期记录拒绝、摘要失配、环回、多 peer、epoch/回退及中央批准身份。
* `control_predicate_windows.cpp`：实际 x86 thunk，合成原生函数体，检验启用/停用/部分恢复及原始 JMP 路径，不使用私人游戏。
* `host_target_cleanup_windows.cpp`：真实 Windows HHOOK 创建/预先移除/清理，连续 12 次复用同一槽。
* `hook_registry_policy.cpp`、`experimental_patch_policy.cpp`、`hook_inventory.cpp`：原有所有权、签名、禁用旧站点、重叠拒绝及清单。
* `feature_lifecycle_runtime.py` 更新历史签名锁存断言；其单成员 MP fixture 无远端传输。需要所有者私人 EXE，未作为本轮无游戏 CI 的已运行项目。
* `.github/workflows/validate-peer-sync-hooks.yml` 编译 x64 Host/x86 Runtime/broker、验证 PE 与跨位 ABI，并上传精确提交的诊断/实验二进制。以实际 CI 结果为准，不把新增测试文本当成通过证据。

未完成：用户平台对 FrameInfo 执行标记的实测、完整原生 Reveal 及后续世界更新、相同配置的旧/新 DLL 双机对照和首次 SYNC 分歧定位。重启工具箱不是新游戏/新模拟基线，不能用它证明已还原以前的改动。

## 合入 17ee488 与本轮实际检查结果

用户随后指定合入 `17ee48810d33d47203d52893d3b0472d992f3a11`。两边共同祖先为 HF32 `9af9852`，不是直接用新文件覆盖全部工作。完整保留该提交的 host_game_input.h、host_input_test.py、原笔记和证据；在 main.cpp 合并其 hostBits/targets/subscriptions 日志与本轮等待/签名字节诊断。来源提交的 64+64 生命周期等结果仍归属于它，不冒认成合并版本已重新运行。

本轮已实际完成：Clang 18.1.6（官方 Zig 0.13.0）对合并后 Host、Assist bridge、GameEntryManager、Runtime、Cursor 及测试的 Windows 目标物件编译；`initial_sync_compiletime.cpp` 的 13 个 static_assert 场景通过；从同一 ABI 夹具的 Windows x86/x64 COFF 常量读取并比对 685 项布局，Assist83/Shared6580496 等基线不变。没有通过链接后的 PE 来替代实际未发生的运行。

CI run 41218221856 的两个 job runner_id=0、steps=[]，没有进入编译；未取得可用 job 日志，不猜测账户原因。本机正常 Zig driver 又遇到沙箱临时文件重命名/快取写入拒绝，因此改用相同编译器的直接 Clang 前端完成物件和静态断言验证。**完整 exe/DLL 链接、Windows HHOOK 测试执行、合并后的 host_input_test.py 运行、PE/import 负例与真实双机测试均未完成**；不发布为已验证联机二进制。

证据：[本轮检查摘要](evidence/peer-sync-hook-review.json)、[COFF ABI 实际数值](evidence/peer-sync-object-abi.json)。尚未完成的运行项不会因为源码中有测试而计为 PASS。

公开布局来源：[EventClass](https://github.com/Phobos-developers/YRpp/blob/d979554/EventClass.h)、[SessionClass](https://github.com/Phobos-developers/YRpp/blob/d979554/SessionClass.h)、[Unsorted](https://github.com/Phobos-developers/YRpp/blob/d979554/Unsorted.h)。既有证据见 HF31/HF32、control-138-regression-audit 与 ordinary-desync-triage；本轮不重新冒认那些旧执行结果。
