# 实验：命令全图采样脱离游戏消息线程（默认关闭）

## 背景、代码路径和证据等级

本次变更基于已通过 Windows x86/x64 CI 的纯显示零页边界 PR #45（main `55bbf6e919aa9768bd6a175c5fa60fcecf15ca81`）。现网现场曾反馈在大量单位时主线程卡顿；未拿到现场性能采样或游戏可执行文件新 SHA-256。本轮是基于现有受支持 YR 内存布局的代码架构审查与受控夹具，不构成实机测量、原生逆向地址或联机正确性新发现。

旧 `world_frame_runtime.h::captureWorldFrame` 在 Assist 消息回调执行完整 `CaptureGraph`。即使 Host 计算已外置，命令需求仍触发大量单位、武器、地图、乘员页面读取。`bridge.cpp::refreshWorldIndex` 在封口样本不全时回退 `indexWorld` 进行 16K 单位的 live 重新扫描；`sticky_bridge.h`、`control_object_owner_bridge.h`、`native_mega_gateway_bridge.h`、`private_outlist_bridge.h` 还有直接调用此入口的独立路径。

## 已提交的实验变更

- 新增 `CommandCaptureWorker`：拥有专用低优先级、DLL 引用固定的 Win32 线程，负责完整 `CaptureGraph`、依赖页排序、RPM 以及临时采样索引。请求采用单槽最新覆盖；新帧、新需求修订立即取消旧采集，避免忙帧浪费；相同工作保留 ticket，避免反复中断自身。
- 采样失败时绝不在消息线程回退到 `CaptureGraph` 或 live 全图。`worldFrameReady=false` 使 `SampleReadScope` fail-closed。既有同步模式逻辑保持不变，避免同帧重复采集回归。
- 完整结果被交给执行线程前，要求同世界 / 场景 / House / 地图 / 连接代次 / session / **完全同一模拟帧**、需求 revision、80ms 交付期限均一致；线程采集前后还独立核验 world/frame/House，采样耗时上限 120ms、8192 次读取、64MiB。调用者最后再进行一次原生 live 身份、权限与帧判定；缺席或失败则拒收/等待下次尝试。
- `Snapshot::swap` 在独占双方时交换所有权，不复制 4KiB 页面字节。但 `indexWorld` 仍会在既有封口结果上为游戏侧建立 `ObjectIndex`，此工作仍在主线程且可能是进一步优化对象；“所有读取都后台”不等于“所有 CPU 逻辑都后台”。
- 消息线程的原生命令/OutList/对象权限检查仍必须在原来所有者线程上；后台结果不会自行生成或推送 EventClass。原生处理路径统一经过 `prepareWorldIndex`：实验模式下只能使用已完成的同帧封口样本，不能再次直接 live 扫描整份单位名单；断开连接依赖 pinned-worker 零等待退出，避免卸载时仍执行模块代码。
- 测试涵盖生产 Snapshot 所有权 swap、需求升级拒收、同帧/连接/House/帧/超时拒收、四个 Hook 全量同步回退入口源码检查、x86/x64 Win32 DLL 工作线程实际 RPM 来源、停止退役，以及保留的 retention/战术状态机和整套编译 ABI。

## 为什么默认关闭，仍需哪些证据

`RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=1` 才开启本实验。完全相同帧的强约束可能让大规模单位时后台采集总是落后当前模拟帧，造成快捷键、自动维修、装车、Ctrl+D 或联机任务久等；如果因此改用跨帧只读候选图，必须逐一完善每种命令的实时 UID/owner/manual-token/mission/目标再验证和原生事件入队准入，不能凭过去的合法状态发送联机命令。运行时的 `indexWorld` 封口快照 CPU 索引、局部类型目录、原生命令合法性、其他非图扫描仍在游戏线程。

本实验目前只完成源码与受控环境测试，**没有通过真实 RA2 1.001 / CnCNet / JJ 平台压力联机的 Ctrl+D、装车、维修和多单位操作成功率验收**。在实际 gameplay P95/P99、命令延迟/成功率、场景刷新/换局/卸载、玩家命令优先级测试完成前保持 PR Draft、不合并 main、不发布 Latest。游戏卡顿不能因受控 CI 通过就宣称消失。
