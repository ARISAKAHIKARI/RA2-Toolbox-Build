# 原生功能依赖隔离与失败后的有界重试（pre.6）

## 问题、基线与证据边界

用户在 pre.5 修复后要求继续排查同类“一项失败，整套功能都卡住”的问题。本轮以已提交的 `d108ab8f976d37e512b183bda95ac2e80f250760` 为反例基线，分别审查 Runtime 全局启动门槛、可选硬件功能的恢复、宿主首次连接和已有订阅的 broker 重建。目标版本为 `1.0.43-pre.6`。本轮聚焦验证已汇总；最终冻结提交、完整编译及交付信息由后续构建/发布 manifest 绑定，不在本记录中预先声明完成。

调查前重读本索引、[pre.5 Ares 入口兼容记录](2026-11-07-ares-native-entry-compatibility.md)、[早先的 ClearBeacons/Reveal 依赖隔离](2026-11-04-platform-startup-dependency-isolation.md)，并只读查询持久 SQLite v147（179 条 notes、178 条 FTS；已有的一条 FTS 漏项保持）。本轮结论将另加新条目，不改写历史记录中的当时判断。

历史现场仍为 Runtime(10).log，SHA-256 `72b05fe7f6c436573171f5f025425d4cbb00ae25fee0157d85fcacc504c88b67`：WH_GETMESSAGE 安装与回调已工作，累计回调 44859 次，原阻断是 `commandStatus=3`。它不能证明本轮新发现的宿主超时或 DR 恢复分支曾在该机器发生。本轮没有新增完整游戏 EXE/模块 SHA-256、现场线程转储或 Windows 实机采集，因此不增加某个新二进制的“已实机支持”声明；日志 SHA-256 不是 EXE 身份。

本轮证据由生产源码审计、旧版反例和生产函数抽取执行组成。Win32、进程、内存、原生调用与断点服务边界使用受控替身；不提交原始用户日志、私人路径、游戏程序或转储。

## 一、21 项原生入口按实际消费者检查

原表包含 21 个原生入口。早先只把 ClearBeacons 和 Reveal 从全局启动门槛移到各自消费者，仍有 19 项可能阻止所有命令进入。进一步按实际调用点审计后，当前 21 项不应共同构成全局 ABI 门槛：18 项属于具体原生功能、2 项已有命令专属校验、1 项只是当前 Runtime 没有直接调用的历史入口。

[startup_dependency_policy.h](../assist/source/startup_dependency_policy.h) 的分类如下；原签名表保留，未知新地址仍默认 `Core`，不会因不在已知分类中而自动放行。

| 地址 | 分类 | 实际边界 |
| --- | --- | --- |
| `0x6AA600` | `Unused` | 历史 StripClass_RecheckCameos 入口；当前 Runtime 没有直接调用，不能阻断无关功能。实际补丁/断点站点仍各自检查。 |
| `0x6ABC60`、`0x5F7900`、`0x4F7870`、`0x6A6300` | `LocalNative` | 建造分类、工厂/建造许可、建造栏项目及科技模式相关消费者。 |
| `0x578080` | `LocalNative` | 地图坐标、队形、路径预览、空中路径点及相关局部消费者。 |
| `0x4C65E0`、`0x4C6860`、`0x4C6970`、`0x4C6B60` | `LocalNative` | 四个原生事件构造入口；构造前检查，失败不填充或入队。 |
| `0x431410` | `ClearBeacons` | 清除信标执行前的即时检查与既有严格 MRGA 兼容策略。 |
| `0x577D90` | `Reveal` | 地图揭示命令及直接 Reveal 组的即时检查与 owned-only 恢复。 |
| `0x6CC360` | `LocalNative` | 超武就绪查询/目录；失败只影响对应原生查询。 |
| `0x4A8BF0`、`0x4AC8C0`、`0x4AC660` | `LocalNative` | 原生侧栏、放置/超武目标光标相关操作，按调用组预检。 |
| `0x48DC90`、`0x6FBFA0` | `LocalNative` | 原生清选、选中及依赖它们的控制/选兵操作。 |
| `0x6D2420`、`0x6D2140` | `LocalNative` | 需要战术视图的镜头定位或坐标投影消费者。 |
| `0x473460` | `LocalNative` | 载员容量读取，失败隐藏容量项而非伪造零乘员。 |

本轮没有新增游戏对象字段偏移或修改原生 ABI。既有 thiscall/fastcall/cdecl 适配、事件字段与函数原地址保持；pre.5 的三处已知 Ares 普通调用策略没有扩展为任意平台或任意 E9 白名单。实际指令补丁、内部比较断点、线程、世界/玩家身份、共享协议及旧 owned Hook 还原门槛继续独立生效。

`bridge.cpp::nativeDependencyReady()` 在每个已知入口保存 250 ms 的成功/失败缓存，到期重查；未知地址返回 false。ClearBeacons/Reveal 原有消费者仍执行严格即时校验。局部失败的处理与语义相符：

- 原生事件在 `fill` 之前检查对应构造函数；失败拒绝本次事件，不把未构造数据送入队列，也不重放已确认命令。
- 有目标的超武操作预检完整目标光标组；无需目标的超武不平白依赖该组。
- 镜头入口只在确实进行该镜头操作、且已有战术视图时要求；普通选兵仍按自身依赖判断。
- 容量失败只隐藏对应容量信息，Iron 等无关项继续；坐标投影失败跳过相关标签，保留仍有效的统计。
- 建造栏已有项目不因不需要调用的 Add 入口而失败；Master 模式也不增加它不使用的重建 API 依赖。科技增强中途刷新与在产保护仍沿现有语义执行。
- Host 日志明确显示 `local-native` 或 `unused-native`，不再把这两类误标为 `Core`。`startup-probe-difference` 仍只报告字节差异；例如可接受的 Ares 入口差异不能直接解释为功能不兼容。

这修正了 pre.5 仍保留的全局依赖范围。它没有替用户启用实验功能、放宽未知原生调用或清除真实 fault/cleanup 状态。

## 二、Full/control 的可恢复失败不再永久锁存

旧 `requestTechParameters()` / `requestControlParameters()` 在 `failure` / `controlFailure` 非零时直接返回。一次暂态入口读取失败会写入 Signature；共享 DR 服务的 System/Foreign/Thread 状态也会写入当时请求组。即使之后服务已经完成退休并回到 Off，功能级错误仍不清除，持续开启的选项只能依赖用户切模式恢复。这与启动签名每秒重查是两个不同路径，不能把旧启动门槛误说成永久缓存。

[tech_ui_breakpoint_bridge.h](../assist/source/tech_ui_breakpoint_bridge.h) 给 Full 与 control 各自增加 500 ms 失败重试节流。只有等待时间满足、共享服务 `healthy()` 且本组 `groupOff()` 后，才重新检查原生入口和世界身份；通过后清除本组失败并重新请求安装。关闭、停止、fault 或离开对应模式会重置本组重试状态。

`groupOff()` 不只看 UI 是否仍勾选：本组 lease、uncertain 或 pendingFor 尚存时不能重新准入。共享服务仍处于 System/Foreign/Thread 时也不能越过保护。已经确认健康的其它组可以继续保有共享 VEH 和槽位；不得为一个失败组擅自清理其它组或外部调试器的 DR。重试发生在维护/请求路径，不增加硬件异常热回调中的重型读取或工作线程。

相关原有站点仍为 Full 的 `0x4F7870` 与两处原生 UI 调用点 `0x6A97C3`、`0x6AA777`，control 参数入口 `0x50B6F0`。本轮改变的是失败后的准入时机，不增加断点地址、不扩充四个物理 DR 槽，也不改变参数与归属语义。pre.5 的 Ares Exact 局部 `-8` 提示和手动选择规则模式的边界继续保留。

## 三、宿主首次连接按失败完成时间退避

旧 `main.cpp::ensureBridge()` 先调用 `gameinput::runtimeAvailable()`，随后才检查 `bridgeBlocked` 和 `bridgeRetryAt`。加载失败直接返回，没有更新退避或失败次数。x64 broker 的启动握手可以同步等待 3 秒，失败清理可以再等待 1 秒；重复失败能够让 Assist 的 UI/控制服务线程反复等待，绕过原有的 2 秒/30 秒退避。这是源码可构造的故障路径，不能直接归因于历史 Runtime(10) 现场。

现在 blocked 或截止时间未到时不进入组件加载；组件失败也计入尝试次数。前两次失败后等待 2 秒，第三次及以后等待 30 秒。创建映射、视图、窗口 token、协议和 Hook 安装失败共用 `bridgeConnectFailed()`，在可能阻塞的加载/安装/清理结束后，以新的 `GetTickCount()` 计算截止时间，防止一次超时把自身等待期耗尽。

原生窗口尚未就绪属于准备状态，保持 250 ms 重试，不增加安装失败次数；该 250 ms 也从探查结束时起算。现有成功连接 fast path、协议拒绝、旧 owner 存活保护、stop=2 后复用、错误代码与安装阶段诊断保留。没有通过重试重新提交已经确认的游戏命令。

## 四、已有订阅的 broker 恢复也保留完整等待期

旧 `shared/source/host_game_input.h::pulse()` 在持有宿主调度 mutex 时，先设 `brokerRetry=now+1000`，再同步重建 broker。若失败耗时超过 1 秒，下一次 4 ms pulse 看到的截止时间已经过去，会立即再次等待。

修复在安装失败返回后用 `GetTickCount()+1000` 重设截止时间。失败保持 `enabled=0`、旧 token/generation 和 Win32 错误，不发布工作；确认成功后仍按原路径更新新 token/generation、恢复当前订阅掩码与节拍。没有在本轮改写共享 mutex 或改成异步 broker 架构：首次尝试仍可能经历既有的单次有界同步等待，不能承诺界面完全无等待。

## 聚焦验证结果与构建边界

[提交内汇总证据](evidence/runtime-dependency-recovery-20261107.json) 的 schema 为 `ra2.runtime-dependency-recovery.validation.v1`。本轮 13 组验证通过，其中有明确数值的当前成功检查合计 **2530**；69 个已记录的当前生产源码/测试文件摘要全部吻合。计数仅包含本轮实际执行并报告数值的成功检查。

| 验证入口 | 本轮成功检查 | 检查对象 |
| --- | ---: | --- |
| `assist/tests/startup_dependencies.py` | 224 | 21 项分类、全局隔离、未知入口默认拒绝、Clear/Reveal 消费者与恢复 |
| `assist/tests/native_entry_runtime.py` | 1125 | 生产启动路径与原有 Ares 可调用策略；合成内存/跳板边界 |
| `assist/tests/native_dependency_consumers.py` | 118 | 实际事件/超武/选兵/显示消费者的局部拒绝，避免无关副作用 |
| `assist/tests/pointer_location_projection.py --portable` | 95 | 空中路径点查询前局部拒绝与恢复；增强查询路径独立 |
| `assist/tests/tech_parameter_recovery.py` | 208 | Full/control 500 ms 恢复、本组退休及其它组保留 |
| `assist/tests/tech_ares_entry_compat.py` | 463 | pre.5 Ares Full 参数回调与 Exact 局部行为保留 |
| `assist/tests/tech_enhancement_midgame.py` | 91 | 科技增强中途刷新、局部依赖拒绝与在产保护 |
| `assist/tests/local_feature_dependencies.py` | 63 | 选兵、磁电及玩家上下文操作的局部事务边界；禁用分支仅显式合成夹具执行 |
| `assist/tests/host_bridge_retry.py` | 64 | 完整生产 ensureBridge 的完成时退避、时钟回绕、正常连接 |
| `assist/tests/host_broker_retry.py` | 30 | 完整生产 pulse 的 1 秒退避、token/generation/掩码与正常节拍 |
| `assist/tests/mo_orders_bridge_test.py` | 49 | MO 命令与局部依赖回归 |
| `assist/tests/formation_quick_bridge_test.py` | 不并入 2530 | 既有整套加 6 个新增场景通过；夹具没有报告总检查数 |
| `assist/tests/control_master_bridge.cpp` | 不并入 2530 | 整套及新增选兵依赖隔离/恢复通过；夹具没有报告数值总计 |

旧版反例单独保留：Full/control 4 个反例按预期失败；宿主首次连接在 64 项中失败 32 项；已有订阅恢复在 30 项中失败 8 项；旧 pointer 地面依赖在第 52 项检查首先失败。这些是对旧缺陷的支持，不能计作新版成功检查。旧 Hook 生命周期 28 项的被提取片段、runner 和顺序与 d108 相同，只继承此前结果，本轮没有重跑，也不加进 2530。

生产函数抽取执行保留所审查的分支与顺序，Win32/内存/传输/Hook worker 替身提供可控时间和错误。Runtime(10) 提供入口字节与目标地址，准入测试中接受的私有目标函数体是合成的。本轮没有打开游戏进程、验证实际 Windows Hook 投递或运行多人会话；真实游戏、Windows Hook 和多人验收标志均为 false。

完整 Windows 目标编译、PE/ABI、打包及游戏验收是后续独立门槛，不属于该聚焦验证汇总。源提交与构建绑定由最终 manifest 记录；本证据文件不嵌入自身提交或摘要，避免自引用。

## 未改的风险与排除的过度结论

### DR 事务的无限等待仍需专门证据与设计

`tech_ui_breakpoint_core.h::Service::start()` 仍等待 DR 工作线程完成，使用 `WaitForSingleObject(worker_, INFINITE)`；等待 API 失败时仍等 job 完成。当前 worker 在 SuspendThread 失败后发布 System 并结束；Get/SetThreadContext 失败仍会走 ResumeThread。仅凭发现 INFINITE 不能推断每次系统调用失败都会冻结游戏。

成功挂起后 ResumeThread 失败，或者 worker 不能继续运行，仍是需要实机线程证据分析的风险。本轮没有这样的现场证据，也没有证明此路径永不挂起。直接加一个超时后让游戏线程返回原生代码，会让仍在更新 DR 的 worker 与旧硬件异常 CONTEXT/lease 并发，可能恢复旧寄存器状态；不能把这种未经验证的退出视为安全修复，故本轮不改 DR 事务协议。

### 存活的 poisoned broker 尚未确认是现场问题

`runtime_broker_client.h::abort()` 尝试终止 broker 后，若 1 秒内仍未 signaled，保留 `poisoned_=true`。`ensure()` 对仍存活的 poisoned 子进程返回 ERROR_BUSY，不会再次发起终止。普通超时且进程退出后会 close 并允许重建；只有终止未完成/失败且进程继续存活的分支存在恢复限制。

本轮日志不能证明该分支发生，故未修改。若后续处理，需要节流重试旧进程退役并明确确认退出；不能直接清 poisoned、重用可能仍执行的旧 mailbox，或同时启动第二个 broker 接管同一游戏连接。

### 已禁用的玩家上下文切换仍混用未知与退休

`player_test_bridge.h::samePlayerWorld()` 把 House 身份字段读取失败、场景/数组身份不同和真实世界退休统一返回 false；`playerMaintenance()` 在 false 时直接清空 `playerContext`，`restorePlayerContext()` 即使没有确认同一世界也会在函数末尾清掉该上下文。若某个允许切换的构建已经从 A 切到 B，一次暂态读取失败理论上可能丢失恢复到 A 所需的原始身份/字段记录。这里确认的是源码中“读取未知”和“已退休”合并的路径，不是本轮 Windows 或游戏复现。

当前产品的 `player_test_policy.h::SwitchAvailable` 为 false，`switchPlayerContext()` 在建立新上下文之前直接拒绝，旧共享请求也不能重开此能力。本轮没有证据将该禁用路径关联到用户的 Hook 故障，保持产品禁用状态；不为了覆盖此假设恢复该功能，也不扩大到修改世界身份/恢复协议。将来若重新开放，需先区分未知/同一世界/已退休，并在未知时保留恢复记录，配合相应受控与实机验证。

### 本轮结论的适用范围

- 已确认并修复的是不恰当的全局依赖、两个硬件功能的失败锁存、两条宿主退避起算错误；不能把它们统一解释成所有历史 Hook 失效的同一个根因。
- 世界/玩家/线程/协议异常和未完成的 owned 还原仍可合理阻止工作，不能为了“界面显示已启用”而伪造成功。
- 局部错误不等于可调用未知平台函数；原始字节差异也不等于 Ares 普通调用一定失败。两种判断都须遵守对应消费者的证据与 ABI。
- 没有宣称已经排除所有挂死、没有将受控替身测试称为 Windows 实机验证，也没有把尚未汇总的 pre.6 交付说成已发布。
