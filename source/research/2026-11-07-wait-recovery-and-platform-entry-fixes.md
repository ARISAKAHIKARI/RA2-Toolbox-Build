# 等待风险、恢复所有权与平台入口适配（1.0.43-pre.9）

## 问题、基线与证据

用户要求处理 pre.6 审计保留的两处底层等待风险，并解释、修正 pre.8 的 15 个 Phobos 潜在交集。基线为已交付 `2051e5c7da24c0df27e9f7bc59978d8994525bdb`（1.0.43-pre.8）。调查前查询了研究 INDEX、owner rendezvous、pre.6 恢复记录、pre.8 兼容记录和 Notebook150；本记录只追加新发现，不重写旧审计当时的结论。

本轮没有取得新的完整游戏 EXE、Ares/Phobos DLL 的 SHA-256，也没有实际运行 Windows 游戏。历史 Runtime(10).log SHA-256 `72b05fe7f6c436573171f5f025425d4cbb00ae25fee0157d85fcacc504c88b67` 只能支持之前 WH_GETMESSAGE 已工作、全局 signature gate 拒绝的结论，不能证明本次两处等待分支曾在该机器发生。受支持的游戏入口和 ABI 继承既有已记录证据；本轮不新增“某个未知 EXE/DLL 构建已实机支持”的声明。

本轮方法包括生产代码审查、完整生产类或生产函数抽取执行、确定性 Win32/内存边界、旧版负对照，以及同提交的 Windows 目标交叉编译。完成的验证与源文件摘要由 [提交内汇总](evidence/wait-recovery-release-validation-20261107.json) 绑定；发布 manifest 另外绑定冻结提交、实际 PE 与归档摘要。原始私人日志、游戏程序和转储不进入用户包。

## 一、异常 broker 从永久忙碌改为有界回收

旧 Client 的 abort 在 TerminateProcess 后等待一秒。如果仍未退出，它保留 poisoned 状态；ensure 看到 alive + poisoned 就只返回 ERROR_BUSY，之后不再尝试终止。第二个问题是 ensure 把 WAIT_FAILED 与已退出一起处理，关闭旧 HANDLE 和 mailbox；remove 又通过 alive=false 把异常/未知状态当作清理成功。

`shared/source/runtime_broker_client.h` 的统一 retire 现在保留 CreateProcess 返回的精确内核进程句柄及其映射、owner、事件。所有异常退役检查都用零等待；每秒最多重新发起一次终止，静默间隔从系统调用完成时起算并处理 tick 回绕。WAIT_FAILED 保留实际错误和原对象身份；只有 WAIT_OBJECT_0 才释放旧资源并允许新 broker generation。poisoned mailbox 不再承载后续请求。shutdown/remove/ensure 共用节流状态和重入保护。

[Microsoft TerminateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess) 说明外部终止是异步请求，挂起 I/O 完成或取消前进程可能尚未退出。[WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject) 区分已通知、超时和查询失败。新实现依此保留待清理身份；操作系统持续不能完成退出时，相关连接仍是局部待清理，而非声称已经清完或并行启动第二个接管者。

实际生产 Client 与 Host clear/reapTargets/pulse 的受控验证通过 175 项（143 + 32）。250 次 4 ms 调度的静默窗内没有新增终止、旧邮箱交换或正时长退役等待。两订阅在旧进程确认退出后共用一个替代 broker 并恢复各自 mask/generation。pre.8 原 Client 负对照出现 44 项失败，单列且计 0 项成功。正常启动、请求交换与一次正常 graceful shutdown 的原有有界超时仍有真实延迟，不能把本修复描述为“宿主所有操作都异步无等待”。

## 二、断点事务的线程等待风险

旧 Service 从 owner 创建 worker，再以 INFINITE 等待 worker 完成。worker 成功 SuspendThread 后若 ResumeThread 失败，owner 自己已无法执行超时/清理代码。仅把等待改成有限超时还会重新引入旧 worker 晚到、Suspend/SetContext 与旧异常 CONTEXT/新 lease 竞态。Microsoft 也明确 [SuspendThread 不适合作为同步手段](https://learn.microsoft.com/en-us/windows/win32/procthread/suspending-thread-execution)。

本轮改为 owner 自己进入 Runtime 中的精确私有异常边界，检查有效 DEBUG_REGISTERS 上下文后事务性提交；使用独立后续边界读回实际寄存器，再确认所有权。第一次功能安装前，在本模块无副作用指令上执行硬件命中 witness，并确认恢复原寄存器。缺少有效上下文、系统未接受写入、未发生真实命中或恢复不确定时，报告局部失败，不回到挂起游戏线程的旧路径。自有异常与未知/外部异常分开，未确认 lease 不执行功能 callback；可能尚存的自有寄存器仍保留处理器与恢复责任。

[Microsoft 的 x86 VEH 示例](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler) 支持修改异常 Context 后继续执行，但没有保证任意软件异常都含有效 DR。本实现因此要求字段和运行时 witness，而不从文档示例推定每个 Windows/调试器组合都支持该机制。具体事务、反例和 Windows 目标编译证据以汇总记录及生产测试为准。

owner 身份通过 DuplicateHandle 保存实际线程内核对象，不因数字线程 ID 被复用而沿用旧 lease。只有该对象明确退出才清理其线程寄存器责任；WAIT_FAILED 不作退出。旧写入存在 A/B 两种可能时，下一次实际读回先收敛到当前值，再记录新的候选 C，防止忽略第二次写入后把仍存在的 B 从恢复记录中丢掉。事务或功能 callback 内不能再次配置/退休；嵌套自有命中只清自己的状态并继续，不再次执行功能 callback。SEH finally 清理请求和执行标记，未知异常继续原处理链。

完整生产 Service 在受控边界下通过 30 个场景、1373 项检查；另保留 6 项实际 AssistExceptionFilter 检查，不重复累计门面重跑的 Service 断言。旧完整 Service 的 3 个无限等待负对照均在隔离测试进程中触发预期超时。两个 Windows x86 fixture 均使用实际生产 MSVC SEH 完成编译和链接，但未执行。调试状态/逐项 DR 准入另通过 245 项检查：System + ERROR_NOT_SUPPORTED 归为环境不支持，显示上下文或实际命中未确认，而非错误标成机器码结构失败；独立签名失败与尚未完成的自有清理仍优先报告。

## 三、发现并修正两类自锁恢复

### 依赖失败被误记为入口本体冲突

旧 controlSitesTick 把已安装站点的多类失败合并为一条分支：上下文签名不符、读取暂时失败、实际写入字节不同，都会调用 markConflict。manager 直接永久标记 conflict；随后正常 restore 又会被 sticky conflict 拒绝，即使本程序写入的入口仍完整。这是独立于 Phobos 的真实逻辑缺陷，可由平台修改依赖范围触发。

现在 controlSiteReady 分开实际入口字节与上下文检查。manager 也独立验证：只有 active journal 的完整写入范围被成功读回、且确实与其 patch 不同，才持久标记冲突。读取失败保留 Pending；缺失/已恢复的记录不能凭调用者失败报告变成虚假恢复责任。真正的外部覆盖继续保持冲突，不拿原版字节去覆盖它。

安装前 actual 与调用者 before 因平台加载发生差异时，尚未写入的站点没有恢复责任，因此此次失败保留重试机会。已 active 的写后字节变化仍是持久冲突。

### 已干净恢复的 journal 不应冻结下一次原值

旧 Registry 在一次正常撤下后仍要求下次 before 等于最初 original；对于用户允许跳过参考签名、且固定编码仍正确的数据操作数，平台在两次安装之间合法变更操作数，也会被旧历史记录永久阻断。

现在只有 active/conflicted journal 保持不可变。一个 cleanly restored 站点重新安装时，仍经过同模块、同 lease、长度、当前策略、固定编码/目标、上下文和 actual==before 检查；通过后按本次实际原值重新建立 journal。恢复读取 manager 保存的最新 before/after。禁止使用旧静态模板撤销第三方改变。

生产 manager + 实际安装/恢复和 control preflight 检查共 155 项通过；原 manager 与原 registry 分别在同一当前 fixture 上触发断言负对照，计 0 项成功。已有 82 个 writer 的 711 项策略回归和 Registry 回归通过。

## 四、15 个交集的准确含义与本轮处理

15 是固定上游 Phobos 的写入范围与本项目目录锚点/已建模校验范围相交的目录行数。其中有 7 行双方声明起点相同；同一 CanBuild 修改可影响多个目录依赖。扫描不是预处理后的 DLL 实际安装表，也不是 15 次现场故障。原始 380 文件、2195 声明/写入记录、固定版本与 Git blob 摘要仍由 [pre.8 静态记录](2026-11-07-injector-compatibility-adaptation.md) 和 [原始证据](evidence/injector-compatibility-adaptation-20261107.json) 保留。

| 本项目目录行 | Phobos 范围 | 本轮判断与行为 |
|---|---|---|
| Call `4F7870` | `4F7877` +5 | 新增已知 +7 Hook 安装/自撤销结构合同；继续原入口调用，验证会被同时恢复的 companion。 |
| Call `48DC90` | 同址 +5 | 新增清选调用合同，保留平台先清扩展选择状态的处理。 |
| Call `6FBFA0` | `6FBFA3` +6 | 新增选择调用合同，保留入口先建立 ESI 与平台过滤返回路径。 |
| Call `6A9316` | 同址 +6 | 提示框依赖，Phobos 可替换 EAX 文本并改变续接；尚无共同文本/布局协议，默认严格校验下局部拒绝。手动跳过参考比较不代表已有语义适配。 |
| Patch `478E4A` | 同址 +6 | 数据操作数 writer 与平台跳转真实碰撞；结构检查继续拒绝，把 E9 当操作数会破坏平台 Hook。 |
| Patch `4AC179` | 依赖 `4AC18C` +10 | writer 不相交，但队列容量/回放依赖改变；仍待语义适配，非普通调用合同。 |
| Patch `55D360` | 同址 +5、退出侧 `55DED5` +6 | 两套暂停/逐帧控制边界真实重叠；需要共同状态协议，保留局部不可用。 |
| Patch `646EF5` | 同址 +10 | 控制命令 owner 与 Phobos 队列扩展实际写入冲突，不能靠关闭签名组合。 |
| Patch `646FD2` | 同址 +10 | 同类队列写入冲突；需要确定构造、容量、入队与回放语义。 |
| Patch `6A97D2` | 共用依赖 `4F7877` +5 | 新增独立严格 CanBuild ordinary-call 依赖判定；本地调用点、参数与续接仍检查。 |
| Patch `6AA781` | 共用依赖 `4F7877` +5 | 同上；普通 Call 的跳过选项不授权此 Patch。 |
| Patch `7E2908` | 同址 +4 | 平台替换航点虚函数指针；需要验证并委托真实原目标，当前固定原目标 adapter 继续拒绝。 |
| DR `4F7870` | 实际入口依赖 `4F7877` +5 | 原指令/入口稳定性独立检查，不继承普通调用准入；平台生命周期变化后重查。 |
| DR `4F7951` | 同一 CanBuild 依赖 | Ares 替代路径可能不经过该内部比较；签名跳过不能制造路径可达性。 |
| DR `4F7B66` | 同一 CanBuild 依赖 | 同上，保持局部路径状态；不把普通调用合同用作内部 DR 许可。 |

本轮为前三个 Call 和两个 CanBuild 依赖 Patch 实施有限适配，共覆盖该静态表中的 5 行。其余 10 行的真实行为/结构/路径约束仍明确记录；没有宣称全部 15 行冲突已消除。

另对本项目自己的目录和 writer/context 做了交叉检查：当前有效 writer 与显式 Probe 的自交集只有 MasterPrune `6AA788` 对 TechPrune `6AA781` 的依赖，二者已有先撤旧模式的互斥转换。侧栏 Render 上下文还覆盖自有视口站点 `4F4492`，现有安装顺序与退休规则单独管理；未据此制造“现场自冲突已复现”的结论。历史 `646E90`、`6FFE00` 已不在当前 writer 白名单。没有加入把任何 E9 或任意保存的原字节覆盖到所有签名读取中的通用放行。

## 五、普通平台调用的有限结构合同

来源固定为 Phobos `ce864b03e56b15f4c59f78955aee87079e54ce76`、SyringeEx `7dbb780dfa7441f8d9b2ae89300540f1cf5f854e`。`native_platform_profile.h` 限定三处既有原入口，识别当前真实 E9 目标、已审查 73 字节 REGISTERS/cdecl wrapper、搬移原指令、覆盖宽度与续接；从 callback 的 MEM_IMAGE AllocationBase 追溯 PE32 模块，核对 `.syhks00` 中精确地址/长度/名称声明、同模块具名导出和加载器能力标志。模块改名不影响归属识别，单有文件名也不能通过。

CanBuild 使用 ECX=house，栈为 type/limitOnly/includeProduction；清选仍为既有无参 cdecl，选中仍从 `6FBFA0` 的对象 thiscall 入口进入。代码只做调用前准入，不跳到平台中间回调。两处科技 adapter 最后仍 push `4F7870` / ret 到原游戏入口，所以可以验证普通调用合同；它们自己的 CALL-site 机器码与周边栈/续接依赖保持检查。`compatibleStrict` 与 raw DR 路径继续读真实机器码。

CanBuild 的 `+7` 合同只接受 `0x4F8361` 为同模块已识别的伴随 Hook，或已恢复的原始 5 字节；该点声明重放 3 字节 `ret 12`，实际入口覆盖 5 字节。主入口、伴随位置及各自跳板在末尾共同复验。

只审查了固定 wrapper 形状。在严格平台准入中，未知附加 callback chain、已识别的导出入口转移编码、错模块/长度/重放/续接、CanBuild companion 不明归属，以及本次读取的主入口、跳板和 companion 快照在末尾复验发生变化时拒绝。入口转移识别并非对整个回调体进行反汇编；末尾复验不涵盖全部模块元数据。负向 profile 的阶段错误通过调试页说明，不只显示一个总的 signature-mismatch。普通 Call 的手动跳过仍按逐项配置处理，不会据此授予依赖 Patch 或 DR 的严格平台准入。

该合同没有对整个 Phobos callback 函数体逐字节认证，也没有绑定用户实际 DLL 的文件摘要。因此可声称“已实现并受控验证有限加载器/入口合同”，不能声称“任意带同名导出的二进制语义已验证”或“所有 Phobos/Ares 版本已实机通过”。现有 Ares 三函数普通调用路径作为既有能力保留，其原有身份与形状局限也没有被本轮改写成更强保证。

## 六、平台 Hook 的预防边界

- 可调用 API、直接机器码 writer、内部 DR 是三种不同消费者；各自判断实际需要的合同和路径，保留独立设置与状态。
- 已拥有的站点通过实际写后字节与 journal 判断，依赖失败不能制造自己的永久冲突；重新安装也不能沿用过期原值。
- 平台普通调用保留原入口及处理链，限定已知布局；同址 writer 必须协商单一所有者或定义可证明的组合语义。
- 没有通用闭环平台适配器。未知 DLL/跳板/回调链只影响其消费者，避免重新进入整个 Runtime 全局启动门槛。
- 失败重试按完成时钟限频；退出、退休和回滚以实测状态确认；未确认的旧对象不能被新接管者重用。
- 本程序可移除自己的无限等待、错误清理与错误准入，无法承诺第三方回调永不阻塞、Windows 内核一定完成进程终止，或未经现场采集的模组任意组合正常。

## 验证边界与未决项

提交内汇总区分当前成功检查、场景、旧版反例、编译和静态扫描，不合并虚假的总通过数。Windows x86/x64 产物与原生 Windows 测试可以在本环境交叉编译；真实 CPU debug-register 行为由运行时 witness 决定，本环境的 API 替身不能代替 Windows 实机证据。尚未执行用户 gamemd-ares、真实 Phobos DLL、平台混合注入、游戏画面、暂停/逐帧或多人会话验收。
