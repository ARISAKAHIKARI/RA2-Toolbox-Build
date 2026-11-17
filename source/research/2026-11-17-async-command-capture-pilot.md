# 实验：命令全图采样脱离游戏消息线程（默认关闭）

## 背景、代码路径和证据等级

本次变更基于已通过 Windows x86/x64 CI 的纯显示零页边界 PR #45（main `55bbf6e919aa9768bd6a175c5fa60fcecf15ca81`）。现网现场曾反馈在大量单位时主线程卡顿；未拿到现场性能采样或游戏可执行文件新 SHA-256。本轮是基于现有受支持 YR 内存布局的代码架构审查与受控夹具，不构成实机测量、原生逆向地址或联机正确性新发现。

旧 `world_frame_runtime.h::captureWorldFrame` 在 Assist 消息回调执行完整 `CaptureGraph`。即使 Host 计算已外置，命令需求仍触发大量单位、武器、地图、乘员页面读取。`bridge.cpp::refreshWorldIndex` 在封口样本不全时回退 `indexWorld` 进行 16K 单位的 live 重新扫描；`sticky_bridge.h`、`control_object_owner_bridge.h`、`native_mega_gateway_bridge.h`、`private_outlist_bridge.h` 还有直接调用此入口的独立路径。

## 已提交的实验变更

- 新增 `CommandCaptureWorker`：拥有专用低优先级、DLL 引用固定的 Win32 线程，负责完整 `CaptureGraph`、依赖页排序、RPM 以及临时采样索引。请求采用单槽最新覆盖；新帧、新需求修订立即取消旧采集，避免忙帧浪费；相同工作保留 ticket，避免反复中断自身。
- 采样失败时绝不在消息线程回退到 `CaptureGraph` 或 live 全图。`worldFrameReady=false` 使 `SampleReadScope` fail-closed。既有同步模式逻辑保持不变，避免同帧重复采集回归。
- 完整结果被交给执行线程前，要求同世界 / 场景 / House / 地图 / 连接代次 / session / **完全同一模拟帧**、需求 revision、80ms 交付期限均一致；线程采集前后还独立核验 world/frame/House，采样耗时上限 120ms、8192 次读取、64MiB。调用者最后再进行一次原生 live 身份、权限与帧判定；缺席或失败则拒收/等待下次尝试。
- `Snapshot::swap` 在独占双方时交换所有权，不复制 4KiB 页面字节；`CommandRoster::build` 也在 worker 从封口的 `0xA8EC78` Techno 列表读取 UID 建立完整 `ObjectIndex`，绝不把递归收集的 passenger/link 队列误当成员名单。丢失任一 UID 即标记未知并拒收；全局磁电载具候选目录也在 worker 构建。工作线程和游戏线程用 `CommandRoster::swap` 只转移哈希表指针所有权，不在游戏线程重新遍历/哈希 16K 对象；仅全局磁电兼容接口仍需复制最多 64 KiB 候选指针。
- `ObjectIndexView` 在命令线程上以已采用的同帧 sealed Key、House 和工作模式为门控访问新的 Hash。无效帧不能退回主线程缓存的旧 World ID 索引；旧同步模式继续使用原固定存储。
- 消息线程的原生命令/OutList/对象权限检查仍必须在原来所有者线程上；后台结果不会自行生成或推送 EventClass。原生处理路径统一经过 `prepareWorldIndex`：实验模式下只能使用已完成的同帧封口样本，不能再次直接 live 扫描整份单位名单；断开连接依赖 pinned-worker 零等待退出，避免卸载时仍执行模块代码。
- 测试涵盖生产 Snapshot 所有权 swap、需求升级拒收、同帧/连接/House/帧/超时拒收、四个 Hook 全量同步回退入口源码检查、x86/x64 Win32 DLL 工作线程实际 RPM 来源、停止退役，以及保留的 retention/战术状态机和整套编译 ABI。

## 继续审查：全局候选模式和过期任务（2026-11-17）

- 修复 **同帧切换“全局磁电候选”模式** 时的交付竞态：交付前不仅校验世界、帧、House、需求修订，还要校验已完成目录的 `global` 标志与当前配置完全一致；不一致拒收，下一次请求按新模式重建，不能误用旧模式的结果。
- 工作线程在任何全图 RPM 前检查请求是否已经超出 **80 ms 交付期限**；扫描中的每次受控读取也检查相同期限，且在较重的 ObjectIndex 构建结束后再次检查。已不可能被游戏线程合法接收的采集立即放弃，避免后台继续占用 CPU。
- 命令消息回调增加 `CommandRequestGate`：始终优先尝试领取后台结果，但对同帧/同需求/同修订/同全局候选模式且 **16 ms 内**重复提交的请求，直接跳过主线程页面列表合并及租约遍历。真正成功入队才更新门控；新帧/House/需求/候选模式改变不受限，暂停帧仍按 16 ms 节流重试。该优化仅减少重复准备开销，未省略必要采样页面。
- Windows 原生 DLL/Host 夹具补充“模式切换拒收、匹配模式成功、过期任务零 RPM”验证。此处的 `80 ms` 只是**过期上限**，不表示 60 Hz 下有 80 ms 的命令执行延迟预算；实际同帧要求仍可能严格到约 16.7 ms。
- 以上仅处理可证明的安全性和无效开销问题；并未证明多单位压力下异步命令采样能及时交付。真实游戏确认前继续保留实验开关，绝不将其视为默认启用或可直接发布的方案。

## 延迟命令执行凭证（统一 nativeMission 入口）

本轮新增 `assist::executionproof::Ticket`，将原始 `{match, house, timestamp, unit ID, unit pointer, manual token, target ID/pointer, destination ID/pointer}` 作为**候选执行凭证**。这不是封包、不是授权缓存，也不是绕过游戏线程的引擎操作。纯验证器不分配内存、不遍历全地图；游戏线程从现有 `worldByID` + `manualVersions` + `own` 获取**当前**身份并拒绝 UID/指针不符、人工接管、对局/所属方变化、目标复用、过期和请求事件不匹配。之后仍经过 `orderpolicy::missionWithTargetTag`、同帧/跨帧任务仲裁、原始 `submitMegaMission` / `submitConstructed` 和队列状态检查。成功入队仅意味着 Queued；`missionObserver` 再独立观察状态。

已接入的真实消费端：

- **AutoLoad**：`compute::LoadInput` 冻结的 `p.object,p.id,tokens[i]` 与 `vehicle.object,vehicle.id` 作为执行凭证交给 `submitProvenMission`，避免后台结果落地前单位/载具身份变化。
- **Unload**：`unloadcompute::Input` 冻结的 `s.pointer,s.id,s.token`，以及选择目标时的原始 ID/指针，在原生最终入口再次验证；保留载员 Limbo 特殊准入与完整三乘员跟进。
- **External planner / Ore / Combat / Magnetron / Defense**：在游戏端采样 `planner::World::Actor::sourcePointer`，Host 规划返回 `planner::Order::unitPointer/targetPointer/destinationPointer`，全部来自**同一原始采样世界**。原始 `manualVersion` 和 `planTick` 不重置；迟到的候选仍在游戏线程复核，指针/UID 复用、人工接管和目标改变必须拒绝。**Assist Host/Runtime ABI 版本从 109 升为 110，必须配套替换。**工具箱自己跟进矿车原生卸载的 `OreHarvestOrder` 不来自 Host，使用该任务冻结的 `j.pointer`、当前匹配的对局以及其自身持续任务截止时间，不误用 Host 截止时间。

注意：普通手动命令和未迁移的老自动功能仍可使用 `nativeMission` 的 legacy 可选凭证分支。当前只是把数条**后台计算结果**改成必须走 proof-required wrapper，而不是宣称所有命令均持有完整原始来源凭证。仍必须遵守零主线程全量采集、误差闭合以及实机 x86/CnCNet/JJ 验证；异步 CommandCaptureWorker 仍以 `RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=1` 试验性启用，不可默认开放。

回归包括：自定义纯 C++ 执行凭证判定（对局/所属方/对象 ABA 指针复用/人工版本变更/目标及目的地替换/事件参数匹配/32 位计时回绕）与生产采矿准入提取式测试、可移植测试和 Windows 原生 ABI 构建。**不把离线单元测试当成真实联机成功率证明。**

## 跨帧持久命令第一步：Ctrl+D（仍为实验性）

**入队不等于执行成功。** 现有 `queuedUnload` 已能跨帧保留显式 Ctrl+D 的载具 UID、手动版本和事件阶段，`nativeMission()` 仍必须在原生游戏线程完成合法性和 EventClass 提交。这轮具体关闭三个会导致“按一次无效 / 越忙越卡”的漏洞：

1. 首次调用 `smartUnloadNow` 失败（无队列空间、临时无效样本、无法提交）时，不再提前置 `Carrier::attempted=true`。只有真的入队普通 Deploy（结果 2）或可靠接收乘员跟进（结果 1）才更新“首次提交已消耗”。同一个明确命令可以在下一游戏帧继续首次尝试，成功后不再重复投递 Deploy。
2. `buildUnloadAdmissionRoster` 过去在一帧把最多 32K 项候选排序，然后逐载具二分查找；现在改为固定内存载具哈希邻接组，在每个候选 **O(1) 期望成本**插入，所有记录仍须执行现场 UID/所有者/手动版本核查，哈希碰撞超预算则整份未知、保留任务而不是发布部分乘员。
3. `QueuedUnload::reindex()` 过去每帧重建全部 8K 队列索引；现在使用双索引渐进重建，每次最多 **128** 条，后台候选目录的所有权和执行时的游戏线程身份核对完全分离。另修复 `repairTick` 在开启 CommandCaptureWorker 时误调用旧 `indexWorld`、清空已交付工作线程索引的缺陷。

4. 异步命令工作线程现在会合并同一对局、同一 House/配置的**正在执行**的短时请求；dispatcher 在组装最多 16K 个请求页之前先调用非阻塞 `deferBusy`。减少 60Hz 下无意义的页面请求复制与后台反复取消、重做。该优化只影响工作请求；`take()` 仍执行**严格同帧**验证，旧帧样本不能成为 native EventClass 执行授权。

**这还是“可靠交付的基础”，不是所有命令的通用异步执行完成态。** `CaptureGraph` 的完整命令样本仍要求与游戏模拟帧完全相同，极端大军团场景存在后台赶不上而无法进入 native dispatcher 的风险。接下来的独立候选侧通道必须允许同一对局、短时跨帧缓存，只充当候选而不充当 EventClass 授权；原生线程必须验证当前 UID、House、人工优先级、队列状态和执行回执。**不能直接取消同帧检查然后把旧快照当作执行权限。**

本轮回归新增：Ctrl+D 首次失败后无需第二次热键即可重试、事件成功入队后不重复 Deploy、原生先卸载后载员跟进、3 名乘员全部建任务、身份复用及过期/主动取消、32K 哈希分组碰撞失败、8K 队列索引每帧 128 条的增量重建。Windows x86/x64 编译与实机联机成功率仍需独立验证。

## 主线程高负载第二轮（仅实验分支）

这轮继续将**可证明的高开销路径**改成后台所有权访问或严格预算任务，而不是为了追求零开销把最终原生游戏引擎调用搬去错误线程：

- **全局磁电**：命令工作线程已生成封口 `globalVehicles`，游戏线程不再为接收结果复制最多 16,384 个指针（64 KiB）；直接使用当前合法的同帧 roster。原生自动登记由过去每帧最多检查 16,384 个候选变成每帧最多 **64 个**，跨帧轮转，逐候选保留实时 House/UID/mission/人工操作优先级校验。不将单帧前 64 个当成完整全局结论。
- **自动维修结果**：原来主线程排序 8,192 个输入 ID 并对 8,192 个结果逐项二分检索，现改用固定内存、无分配、代际哈希的分段成员证明；每次回调最多 **4,096 次**哈希插入/查询，最坏四次回调。所有结果验证完成前不进入 `repairCount`；ID 冲突超预算、缺失输入、换局、过期、聊天中断均 fail-closed。**250ms/12 帧**新鲜度约束不放宽，因此最坏规模仍需要真实联机压测检验交付成功率。
- **科技目录**：指纹采集缺页/失败时不再每 100ms 触发全量 `scanTypes`；保持最后一次完整目录，等待下次轻量指纹成功，确认变化后才重新扫描。新对局冷启动目录尚未后台化。
- **回归覆盖**：16K 磁电轮转覆盖/预算、8K 维修输入与结果、拒收非法 ID、结果顺序及重复保持、取消不发布部分结果、32 槽有界探测碰撞失败；原有纯显示、WorldFrame/缺页、native x86/x64 构建继续执行。

### 仍必须继续拆、暂不宣称全局完成

| 优先级 | 当前游戏线程路径 | 未完全解决的原因 |
| --- | --- | --- |
| P0 | 默认同步命令 `CaptureGraph`、`indexWorld` 及 `ManualVersions::copyLive` | 实验异步路径避免整表同步扫描，但同帧交付可能在 60Hz 大军团下饿死；未通过真实 Ctrl+D、联机、自动装车验收，不能贸然替换默认执行链 |
| P0 | `observeUnloadRoster` 和 `buildUnloadAdmissionRoster` | 每帧可能遍历 16K infantry、对最多 32K 的候选列表排序；需要封口乘员目录、事件历史及分帧消费的配套设计，不能直接截断造成乘员丢失 |
| P1 | `collectRepairs` 及多个战术意图的原始枚举 | 本轮只去掉结果端 8K sort；最初的 8K 主线程观测需要转入 Host/后台并在投递前核验 |
| P1 | `scan` 的全局单位状态与原生画面投影、`scanCrates` | 纯显示主图数据已有 Host 通道，但本地原生显示路径仍可能 16K 全表扫描或逐对象调用原生引擎；需要只采变化和有界投影 |
| P1 | `scanTypes` 冷启动与同步类型构建 | 已消除缺页导致的反复重扫，但首轮扫描仍需封口类型目录和按需投影 |
| P2 | native order/卸载/观察者的其他 8K 维护遍历 | 应分别引入游标、固定回调预算和命令优先级，避免任何单个自动化功能长期占用同一帧 |

CI 通过只说明编译/夹具成立，不证明游戏主线程 P95/P99 已达标。下一阶段应持续测量每模块时间（含入口、候选合并、页面/索引、执行与绘制）并确认各类命令成功率，优先消除 P0 遗留风险。

## 为什么默认关闭，仍需哪些证据

`RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=1` 才开启本实验。完全相同帧的强约束可能让大规模单位时后台采集总是落后当前模拟帧，造成快捷键、自动维修、装车、Ctrl+D 或联机任务久等；如果因此改用跨帧只读候选图，必须逐一完善每种命令的实时 UID/owner/manual-token/mission/目标再验证和原生事件入队准入，不能凭过去的合法状态发送联机命令。命令用的整份 `indexWorld` 封口快照 CPU 哈希构建已经移到 worker，但 `ManualVersions` 原有达到阈值后的清理/重排尚未改成独立增量方案，连续极长对局可能存在表容量风险；局部类型目录、最终原生命令合法性、部分非图维护仍在游戏线程。必须继续逐项审查这些成本，而不是宣称整个 Runtime 已没有同步工作。

本实验目前只完成源码与受控环境测试，**没有通过真实 RA2 1.001 / CnCNet / JJ 平台压力联机的 Ctrl+D、装车、维修和多单位操作成功率验收**。在实际 gameplay P95/P99、命令延迟/成功率、场景刷新/换局/卸载、玩家命令优先级测试完成前保持 PR Draft、不合并 main、不发布 Latest。游戏卡顿不能因受控 CI 通过就宣称消失。
