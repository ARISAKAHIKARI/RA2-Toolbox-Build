# 控制全部第四轮：普通权限直接 Yes，资格改在维护阶段发布

## 问题与依据

基线为已交付 `1.0.44-pre.3`，提交 `b9e7111391f9ffcbf20d3a1baf785498a8792155`。用户继续要求优化“控制全部”，并明确指出“我们不是相当于直接判 yes 吗”。本轮据此调整控制权限的处理方式：普通调用直接选择已核实的原生 true 叶，世界资格由正常维护决定，不再把旧版逐命中身份检查作为必须保留的业务语义。

调查前已检索研究 INDEX、[控制原生 ABI](2026-11-01-master-control-native-audit.md)、[单槽硬件控制](2026-11-04-control-hardware-parameters.md)、[第三轮热路径](2026-11-08-hardware-precomputed-tech-and-owned-dispatch.md)以及同一持久 SQLite 笔记 v161。沿用支持样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的既有原生依据，本轮没有访问或发布私有游戏 EXE/dump，没有新增游戏地址或平台支持声明。

| 既有位置 | 已核实语义与本轮使用 |
| --- | --- |
| 入口 `0x50B6F0` | 单个执行断点；ECX 为 House，`[ESP]` 是原 CALL 返回地址 |
| 普通叶 `0x50B710` | `mov eax,1; ret`，不解引用 ECX |
| 严格叶 `0x50B716` | 读 `CurrentPlayer[0xA83D4C]`，与 ECX 比较，返回 EAX=0/1；不是 House 对象有效性检查 |
| 返回地址 `0x6339BF` / `0x637E45` | 两处所属方编号查询继续选择严格叶，避免外方编号被折成本方 |

## 一、直接选择原生叶，不再重复计算世界与目标身份

旧 tag4 成功路径先检查十个世界字段，再读目标 House 的数组、数量、索引、槽指针、vtable 和 UID，最后读一次返回地址，共 17 次游戏/栈字段读取。pre.3 虽然已消除读取器间接调用，这些检查本身仍在每次异常中执行。

现在 tag4 保留 control policy revision、已发布 `policy.controlling`、Shared 存在、当前硬件模式、stop/fault，以及 ESP/返回地址范围检查。然后只读取一次 `[ESP]`，对两处特殊 caller 选择 Strict，其他有效 caller 选择 Allowed。没有新建目标 House 候选表；原生两个叶都不读取目标 House 字段，因此回调直接移除六项目标身份检查。

回调仍只改 CONTEXT.Eip，原 native CALL、返回地址、ESP、返回过程、严格叶的 EDX/EFLAGS 行为由游戏原生代码完成。不在回调内仿写 `ret`，不写 Owner、CurrentPlayer、House 控制数据或游戏代码段。调试寄存器的认领、DR6 清位、RF、foreign/mixed 交付和嵌套抑制保持原 Service 语义。

17→1 只计算 tag4 回调自身的游戏/栈字段读取；严格叶恢复执行后仍有它原来的一次 CurrentPlayer 全局读取。许可关闭或检查失败时，回调不重定向，继续原生谓词；回调自身返回 0 不等于强制游戏权限结果为 false。

## 二、把“槽已安装”和“当前世界资格”分开

新增 owner-thread `controlEligible`。`configure()` 只有在 desired、没有控制失败、eligible 和 `ready(16)` 同时成立时才发布 `policy.controlling`，其他功能的配置不能仅凭已有控制槽重新发放已撤销的许可。

每次正常 `maintainTechParameters()` 在节流之前先刷新资格。刷新先清 eligible 并撤销 controlling，再进入专用 `GuardedControlWorldEligible`，使用原来完整的十字段 world 谓词。启用请求在 `captureWorld` 之后也刷新资格；世界代际变化、模式/stop/fault、策略变更、恢复和 Service 故障均参与撤销。存在寄存器事务 pending 时保守保留未发布状态，原 DR 责任仍在，下次正常维护可重新发布，不建立永久失败状态。

冷端读仍是在局部 AV/in-page SEH 范围内的即时 volatile 字段读取，没有把这十次读改成十次 ReadProcessMemory。新 guard 保留 LastError，并沿用 `noReportAccess`；无关异常继续传播。刷新前先撤销许可，因此无关 unwind 即使跳过 configure，也不能留下原有成功资格。

### 这是一项明确的行为调整

旧版逐次检查世界状态。新版以最近一次正常维护结果决定世界资格：世界字段在两次维护之间改变，普通权限路径仍可能直接 Yes，直到下次维护撤销。维护由现有游戏线程消息/工作入口驱动，没有固定毫秒上界，不能写成“严格每帧”或“立即检测所有世界变化”。当前模式、stop/fault 和策略版本仍逐命中检查。

目标 House 无效指针也不再由回调主动排除；这符合直接选择原生叶的契约，两个叶不解引用目标对象。它不表示任意上层游戏调用都能安全使用无效对象，也不构成联机同步安全证明。严格叶继续读取当时的 CurrentPlayer，不能用维护时的玩家指针仿写其结果。

## 三、物理槽的固定掩码提前确定

四个物理槽的 DR7 mask/enabled 是不可变值。把原每次计算替换为两张共 32 字节的 constexpr 表；每次分类仍检查当前 DR7、DR0–DR3、EIP、lease、uncertain 主/备选身份以及 foreign/mixed 状态。

固定 pre.3 对象与实际前后 Win32 x86 `-O2` 探针对照：classify 静态指令 197→175，可变移位 8→2；正常 feature 槽路径省掉三次可变移位。entry 46、body 9、cleanup 4、dispatch 109 均未变，无新增调用。switch 方案被编译成同类查表；展开 actualTag 会引入额外调用，本轮未采用。

这项机器码计数不包含 CPU/Windows 异常派发与恢复成本，不等于硬件异常延迟或 FPS。原生 Service 计时只作为受控分发比较保留原始分布，不对外宣称加速百分比。

## 四、验证与交付绑定

实际测试命令、原始输出、源码 SHA-256、机器码与独立审查结果见[相邻证据 JSON](2026-11-08-control-direct-permission-hotpath.evidence.json)和最终验证包。本轮把旧版逐命中 world/House 拒绝与新版维护许可行为分别验证，不将新契约误报为旧版全部读取/故障轨迹完全相同。

重点覆盖普通/严格 caller、零目标与损坏目标记录、同维护周期多次命中、即时停用、坏栈、维护前/后世界改变、每字段读取失败、AV/in-page 和无关异常边界、失败后 sibling configure、pending 恢复、世界代际与策略变更。四槽 Service 比较的固定前后同夹具均为 47 场景、10,019 checks，包括 1,568 次四槽矩阵 receiver 交付。

最终专项收据共八组，其中五组执行受控夹具、两组仅编译链接 Windows x86 SEH 夹具、一组检查实际完整 Runtime/guard 编译对象。独立只读审查作为补充证据绑定，不算第九组执行测试。

| 专项组 | 本轮实际结果 |
| --- | --- |
| owned Service 前后比较 | 各 47 场景、10,019 checks；实际 pre.3 对象与固定源码生成的五个 Service 函数一致；当前 classify 静态指令 197→175、可变移位 8→2 |
| 控制直接路由 | 130 cases；106 个明确预期改变的结果；成功 tag4 回调读取 17→1，不声称逐命中 live 等价 |
| 冷资格生命周期 | 1,369 checks，170 次热回调、234 次冷 guard、131 次维护；完整冷 world 保留十字段读取 |
| Control 与 Full 既有夹具 | 2,752 checks 通过；覆盖严格 caller、原字节准入、无数据写入及即时门控 |
| Full/Enhancement 候选回归 | 当前 132,826 checks、322 次回调；其中 131,136 个 flags 差分用例；此前科技路径未改变 |
| Control SEH | 整个实际 `guard.cpp` 的 Windows x86 SEH 夹具编译和链接成功；没有运行 Windows 可执行文件 |
| Enhancement SEH | 实际 guard 头与新增冷资格符号依赖编译和链接成功；没有运行 Windows 可执行文件 |
| 完整 Runtime 机器码 | 实际 `bridge.cpp -O2` 对象中 tag4 本体静态指令 45→33，旧 world/House 两次调用→零，当前分支无调用；冷原生 SEH、LastError 恢复及刷新前先撤销许可的写入均保留 |

初次请求时 running/observer/dead/defeated/observerHouse 无效的五类情形均确认不发布许可；这时请求状态仍可能返回 Parameters，只表示 DR 已就绪。模式在维护前 Off→On 可以复用原有发布；维护已经观察到 Off 后，On 必须重新请求准入。这两种时序分别有用例，回调拒绝仍表示继续原谓词。

最终源码冻结、完整 Windows 目标构建、静态 PE/ABI/版本和更新包散列由交付 manifest 记录。该证据不代表 Windows 实机、真实 #DB/VEH 全链延迟、游戏/GPU/FPS 或多人同步运行。

## 五、保留范围与未决问题

本轮不改断点地址、不轮换槽位、不自动退回代码补丁；实际异常触发次数没有减少。Service 的 GetCurrentThreadId、Guarded 回调的 LastError 和两层各自负责清理/访问失败处理的 SEH 仍保留，避免把业务直接 Yes 扩大为任意调试异常也直接处理。

真实现场卡顿还受公共入口命中频率与系统异常成本影响；当前证据不能量化它们占比。把检查移出回调后，应由实机日志中的 owned/control 命中窗口和帧时间判断剩余瓶颈，不能把软件调用/字段读取的减少直接乘成整体帧率收益。
