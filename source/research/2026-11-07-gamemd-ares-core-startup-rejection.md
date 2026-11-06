# gamemd-ares：消息 Hook 正常，三个建造入口阻断核心启动校验

## 问题与调查范围

用户回传 Runtime(10).log，报告本次 gamemd-ares 的 Hook 功能失效。调查前检索 source/research/INDEX.md、平台启动依赖隔离与 Reveal 包装器记录，并查询持久 RA2-Reverse-Engineering-Notes.sqlite v144。既有平台采集记录的 19 项核心入口均匹配，未找到本次三个核心私有跳板已获验证的兼容记录。

本次仅作日志与生产源码分析，未修改生产准入、执行平台跳板或生成新二进制。

## 现场身份与证据

- 日志 SHA-256：72b05fe7f6c436573171f5f025425d4cbb00ae25fee0157d85fcacc504c88b67；1061 行，客户端时间 2026-11-07 02:40:08.491–02:49:09.282。
- 日志产品标记：v1.0.43-pre.3；目标 gamemd-ares.exe，已加载 MRGA2.dll 与 Ares.dll。这里只保留模块名与诊断摘要，不提交用户路径、原始日志或私有内存。
- 未提供本次 EXE、Ares/MRGA 模块或 GameRuntime.dll 的 SHA-256。PE 时间戳/映像大小和目录名不能替代文件身份；不据此宣称新增支持某一映像。
- L10：WH_GETMESSAGE 安装成功；L13：connected 且回调已发生；L23：bridgeStatus=1、stop=0、blocked=0，但 gate=signature-mismatch、gateCode=3。
- L1041：callbackCount=44859、callbackTid=expectedTid=15940、callbackAgeMs=15、fresh=1、commandStatus=3。L1045：游戏已推进到 frame=27818，仍是相同 gate。
- 119 条 feature-activation 中，110 条 signature-mismatch，9 条 initial-or-no-world，没有 ready。189 条 bridge-health 的 runtimeError、faultSerial、exceptionCode、nativeExceptionCount、postError、hookConflicts 均为零。日志没有记录解释本次失效的运行时异常。

## 三个核心字节差异

L19 明确 core byteDifferences=3、unreadable=0、compatibility=host-not-evaluated。L20–22：

| 入口 | 已有函数身份 | 预期前8字节 | 实际前8字节 | E9目标 |
| --- | --- | --- | --- | --- |
| 0x006AA600 | StripClass_RecheckCameos，建造栏条目复核 | A06BEDA80081EC44 | E9FB59C60781EC44 | 0x08310000 |
| 0x005F7900 | ObjectTypeClass::FindFactory | 83EC08535556578B | E9FB866F0656578B | 0x06CF0000 |
| 0x004F7870 | HouseClass::CanBuild | 83EC3C8A44244453 | E98B87DE04909053 | 0x052E0000 |

三处目标均 MEM_COMMIT、MEM_PRIVATE、PAGE_EXECUTE_READ（0x20）、4096 字节、not-in-module-snapshot；五组 core 探测内容一致。这是读取成功后的字节变化，不是读取失败。

函数身份依据：
- 本库 [科技原生路径记录](2026-10-22-tech-131.md) 已确认 FindFactory：thiscall，allowOccupied/requirePower/requireCanBuild/house，类型虚表 +0x94；CanBuild：thiscall，type/buildLimitOnly/includeProduction。
- Phobos 官方维护的 [Ares 3.0p1 Hook 清单](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/.agents/skills/check-hooks/ares_3.0p1_hooks.cpp) 的 302/430/603 行同时列出这三个地址。该清单证明这些是已知 Ares Hook 站点，不证明本现场私有页的生成者、处理程序或完整 ABI。
- 本次没有新增字段偏移或实机调用证据。0x6AA600 的具体调用 ABI 未在本轮验证。

## 生产源码为何拒绝

核对提交 9966efd2020543395ffa555bf0aabfa6d8a34502（pre.4）；其启动校验相关文件与日志版本 pre.3 提交 1d7f3020b12c8f25b8b83e161ddd1bafc6364cda 相同。

bridge.cpp 的 startupSignature 首先读入口并与原字节比较；失配后，只要地址不是 0x431410（旧 MRGA1 清信标特例）就立即 false。三个现场 Core 入口都到达这一拒绝条件。此路径根本没有解析它们的 E9 目标，也没检查目标的 MEM_PRIVATE/MEM_IMAGE 类型。不能把原因说成“明确因 MEM_PRIVATE 类型被拒绝”；目标区域类型只用于旧信标兼容分支。

startup_dependency_policy.h 仍只将 ClearBeacons 0x431410 和 Reveal 0x577D90 拆为可选依赖，其余 19 项由 coreReady 逐项检查、任一失败返回 false。Signatures[0] 就是 0x6AA600，当前按顺序先在这一项短路；另外两处也分别不能通过现有校验。bridge.cpp 的启动路径在失败时每秒重查签名；原入口持续保持 E9，所以重查继续失败，不是一次失败后永久缓存为拒绝。

因此，直接阻断原因是“建造相关入口仍要求原版字节，并属于全局核心门槛”。消息 Hook/IPC 继续工作，而多项已请求功能保持 requested/applied 不相等，如 tech=5/0、techEnhance=1/0、placement=1/0、map=2/0。match 是 sessionTracker.epoch（对局/上下文世代），不是签名匹配计数。

## 与以前修复及重连的关系

[2026-11-04 依赖隔离](2026-11-04-platform-startup-dependency-isolation.md) 仍在：清信标与 Reveal 的未知替换不再单独阻断全局。本次另有三个核心地址发生变化，超出那次修复范围，不能说旧可选隔离被回滚。

本次另两处可选差异仍存在：清信标 0x431410 指向 MRGA2.dll RVA 0x1595B0，Reveal 0x577D90 指向私有页 0x06830000。只有清信标目标可由本日志直接归属 MRGA2；三处 Core 私有目标的实际归属未决。

02:41:47–52 工具箱进程换过，但游戏 PID 相同；02:43:07–15 同一工具箱对同一游戏连续八次 detach/重装，游戏均 stillActive=1，每次重装后仍 signature-mismatch。日志未说明这些 detach 的触发原因，不能认定为用户手动或自动误卸载。反复重装没有改变三处入口字节，因而没有解除门槛。

末尾 02:48:54 的 Win32=299 与模块头不可读紧邻游戏正常退出（exitCode=0、stillActive=0），不解释此前持续失效。旧目录名中带 1.0.41-pre.4 不能单独证明 DLL 混版。

pre.3→pre.4 的改动是蜘蛛工作恢复、科技中途刷新及相关版本/记录/测试，未修改本次 startup 校验。因此 pre.4 本身不能解除这组 gate。

## 后续修复边界

三处主要关联建造、科技和侧栏，但消费者不止科技全开开关：包括预约生产、生产命令、模式切换的在产保护和操控方切换侧栏等。按消费者隔离需要覆盖这些实际调用与维护/恢复路径，不能仅从 Core 表删去三项。

Full/Exact 科技模式还分别通过 nativeCompatible/enhancementCompatible 检查 CanBuild 入口。即使解除全局门槛，这些功能仍可能局部拒绝。因此“避免无关功能被连带停止”和“科技功能实际兼容 Ares”是不同完成标准。

日志缺三处私有页本体、后续跳转/handler、稳定性和对应模块身份。已知 Ares 站点与现场形态一致，使 Ares 入口兼容覆盖不足成为有根据的判断；仍不能据此声称现场三个替换函数均已通过 ABI/语义验证。下一步完整适配需要同一运行进程的只读平台采集，覆盖三处入口、目标页及 handler 链。静态磁盘 EXE 单独不足以反映这些运行期改写。

本轮结果是确定的拒绝路径诊断，不是 Windows 实机功能修复、联机验证或新的兼容授权。
