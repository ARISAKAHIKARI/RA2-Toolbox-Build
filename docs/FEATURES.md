# RA2 Toolbox 1.0.2 · 完整特性与实现说明

版本：1.0.2；Windows 文件版本：1.0.2.0；Assist 协议：42；Runtime 协议/API：3。EXE 与 GameRuntime.dll 必须配套更新。

## 1.0.2：Shift 连点与冲突全执行

修复过期 Hook 句柄使设置处理器误关 Shift 五连点的问题。旧配置若已被误关，重新勾选“Shift 五连点”即可恢复；保留明确保存的关闭值。低显示帧率下仍以最多 4 ms 的目标间隔观察按键，复用原有计时器，高帧率显示继续按自己的频率绘制。自动模式发现 Ares 时使用原生五连点；需要额外补点时可选“强制补点”。

战术面板新增“热键冲突时全部执行”，默认关闭，即时生效并自动保存到 `[Hotkeys] ExecuteAllMatches`。开启后，键盘组合只注册一次，再依次执行全部匹配动作；十项增强鼠标功能也逐项匹配并共用同一次输入捕获。后续请求等待已有命令、选兵或协同回执，防止覆盖邮箱。仍保留每帧额度、所有权、目标、时效和取消检查。

同一单位的后续命令可能覆盖前项，持续规划仍只保留最后一项；全部提交不等于多个互斥任务能同时持续。系统中其他程序占用的快捷键仍可能注册失败。Assist 协议为 42，EXE 与 DLL 必须整包更新。详见 [连点与冲突处理](HOTKEY-AND-SHIFT5.md)。

敌方标记去掉名称前的 P1/P2 等玩家编号，保留单位名称和玩家颜色位置符号，刷新率不变。

## 继承 1.0.1 的敌方位置标记

战术面板的显示设置勾选“敌方位置标记（包含未揭示区域）”即可使用。默认关闭，即时生效并自动保存到 `Assist.ini` 的 `[Display] EnemyMarkers`。

按玩家颜色标出当前战场视口中的敌方单位和建筑，包含未揭示区域；建筑使用方框，其他单位使用菱形，并显示类型名称。排除己方、盟友、中立与已进入运输工具的乘员。仅显示游戏内存中已有且布局兼容的数据。

刷新率与现有覆盖层一致：共用上方 FPS 设置（5–500）、同一采样时钟、同一 Motion 插值和绘制窗口；同一逻辑帧内也更新位置。名称与文字位图缓存，场景遍历按原有逻辑帧/镜头刷新规则进行。最多 2048 个标记；菜单、失焦、跨局、读取失效或数据过期时隐藏，不绘制到侧栏。

仅使用统一外部读取器，不修改战争迷雾、不调用地图揭示、不写游戏状态、不提交事件或新增 Hook。详细说明和验证边界见 [敌方标记说明](ENEMY-MARKERS.md)。

## 继承 1.0 的单位清单配置

战术助手面板找到“功能单位清单”，选择功能，填写逗号分隔的单位 ID，再按需勾选“仅处理自定义清单中的类型”。未勾选时沿用原有自动识别；勾选且留空则不处理任何类型。设置可离线修改，即时生效并自动保存。

每项最多 512 个 ID，每个最多 23 字符，可用英文、数字、下划线、短横线；自动归一大小写、去重，支持中英文逗号、分号与空白分隔。输入无效时保留上一次有效配置。各项互不覆盖。

| INI 键 | 用途 | 保留条件 |
|---|---|---|
| `Aircraft` | 飞机分配攻击 | 仍须属于原版飞机/空中载具并有兼容武器 |
| `Disk` | 飞碟主武器 | 保留飞碟/吸取副武器识别 |
| `Magnetron` | 磁电轮换 | 仍需磁力武器与 Locomotor 弹头 |
| `AdvancedAttack` | 高级移动攻击 | 仍需合法地面单位与武器 |
| `LoadPassenger` | 自动装车乘员 | 仍需原版进入许可与体积兼容 |
| `LoadTransport` | 自动装车载具 | 仍需原版载员容量与进入许可 |
| `EnterSource` | 高级进驻乘员 | 仍需目标允许进入/占领 |
| `EnterTarget` | 高级进驻建筑或载具 | 分配屏幕内同类目标，仍检查容量 |
| `UnloadTransport` | 智能卸载载具 | 发原版整车卸载任务 |
| `UnloadPassenger` | 卸载后乘员任务 | 筛选后续部署/占领/攻击，不改变整车卸载 |
| `EscapeTransport` | 逃生载具 | 仍只跟踪选中的己方载具 |
| `EscapePassenger` | 逃生乘员 | 不生成幸存者，原版允许部署才发送 |
| `SellVehicle` | 变卖战车 | 仍须己方且满足原版格子/建筑条件 |
| `RepairBuilding` | 维修建筑 | 与不可维修建筑许可分别校验 |

以上项保存在 `Assist.ini` 的 `[FeatureTypes]`：`AircraftCustom=1` 表示启用飞机清单，`Aircraft=ORCA,BEAG` 是 ID 列表；其他项使用对应键名和 `Custom` 后缀。示例中的飞机、飞碟、磁电初始文本为 ORCA/BEAG、DISK、TELE，默认自定义开关均关闭，不能视为 MOD 的完整单位清单。

矿车和矿场继续使用 `[OreTypes]` 的独立开关/清单；自动 CS 继续在单位配置中逐类型选择，组合选兵继续用相同快捷键成组。维修默认尊重原版 `ClickRepairable`，需要放宽时另外勾选“允许维修不可维修的建筑”。

宿主重连、对局结束与新局建立时清除配置缓存，重新读取宿主快照，即使新宿主复用了配置序号也不会沿用旧清单。

清单只筛选本工具处理的类型，不修改原版属性、武器、归属、储矿/载员容量或 Dock 关系。名单在输入捕获和最终发令前复核，旧计划被新配置排除后不会继续提交；已入原版队列的事件不会被工具撤回。卸载/逃生的旧跟踪会停止，必要时沿用既有 Guard 清理释放本工具曾提交的 Sleep。

## 默认操作与配置方式

多数普通动作默认 Ctrl+Alt+对应字母，当前值以面板中实时显示的快捷键为准；旧 INI 会保留用户设置。组合键可留空，增强鼠标键每个功能分别选择停用、中键、侧键 1、侧键 2、右键。这里没有新增可配置优先级仲裁。

- 自动 CS：启用并勾选类型后，正常右键移动；观察到原版 Move 后追加 AttackMove。
- 按键 CS、高级移动、磁电：各自组合键 + 各自增强鼠标键。配置为右键时保留原版点击，观察原版移动后接续。
- 高级进驻：默认 Alt + 中键点击建筑或载具，按屏幕内同类目标容量分配。
- 飞机分配攻击：按各模式配置键和鼠标键点击敌方目标；两种模式的受击配额独立，目标类型配额可覆盖默认值。
- 飞碟主武器：默认 Ctrl + 中键点建筑，提交原版对地攻击。
- 合法己方变卖：默认 Ctrl + 侧键 2；载具仍受原版出售条件限制。
- 自动装车、智能卸载、一键维修、选兵和超武：使用面板按钮或已配置快捷键。
- 单位统计：Tab 模式可选，也可用 Ctrl+Alt+H、显隐按钮或不依赖 Tab 的常驻模式。

## 整体架构

宿主 EXE 的三大模块共用读取连接管理；外部采样独立缓存、并行调用读取，不以整批串行锁包住采样。类型目录、图标和配置可跨对局保存。外部规划器计算战术目标、返矿排队与协同分配，附带来源时间、帧号、对局和手动操作代次。

游戏内仅使用一个 GameRuntime.dll，经同一管理器的 WH_GETMESSAGE / WH_CALLWNDPROCRET 进入验证过的游戏 GUI 线程。功能没有各自的 Hook，也不改写原版函数入口、任务事件或网络接收逻辑。Windows 消息订阅仍然是 Hook；“只有统一管理器”不等于完全没有 Hook。

发令统一调用原版构造器与提交入口：单机 DoList，多人 OutList；选兵、镜头、光标、侧栏提示等本地 UI 走对应的管理器接口。选兵邮箱与模拟任务邮箱的区分用于避免等待，二者共用同一线程入口。自动装车分配、维修节拍及部分卸载/逃生选择仍留在 Runtime，不声称所有业务计算均已迁出。

每帧总事件额度为 24，繁忙时为其他任务留额度。外部战术计划最长 150 ms / 6 帧，协同计划 250 ms / 12 帧；增强输入有独立的捕获/接续时效，来源时间不会在提交时刷新。队列忙保持未处理部分；过期、跨局、身份复用或手动接管时拒绝。入队回执与实际观察完成分开，不能把“已入队”当作“任务成功”。

## 全量功能目录

下列编号保持既有版本的对应关系。停用、实验性与仅外部展示项均明确说明。

### F01 · 公共游戏发现与窗口定位

枚举兼容进程，先确定 PID，再读 YRpp 的 Game::hWnd 并核对窗口所属 PID 和线程。指定进程暂时没有窗口时保留只读连接，不能改绑另一场游戏。采用显式 read 字节读取，避免把 get 返回值误当输出参数。 光标模块也复用公共游戏名、窗口发现和 YRpp Game::hWnd 校验；保留同 PID 启动器子窗口路径。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/process_discovery.h；message_query.h 位于 shared/source。

### F02 · 统一外部读取器

战术、单位统计和光标统一使用 ReadSession。同一 PID 和访问能力共享受引用计数保护的只读连接，缓存、失败诊断和批次计数归各任务独立所有；仅连接登记加短锁，读取和整批采样不持全局锁。统计/战术使用批次页面缓存，光标使用 readFresh 即时读取。只查询连接与可读连接隔离，关闭一个模块不关闭其它模块的连接。进程退出后重新连接生成新代次并清空该会话缓存。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/process_connection.h; process_reader.h; shared_reader.h。

### F03 · 布局检查与分层就绪

布局探针检查关键对象与数组；readStatus、bridgeStatus、commandStatus、renderStatus 分别表示读取、桥接、命令和显示。能显示不代表能下单，命令桥异常也不应直接抹掉有效的只读显示。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/layout_probe.h；assist/source/main.cpp 的 mergeExternal。

### F04 · 单一 GameRuntime 与游戏线程管理

EXE 通过统一 host_game_input 订阅消息入口；只有 GameRuntime.dll 进入游戏。游戏线程租约与忙碌拒绝保留，忙碌不伪造回执。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：shared/source/host_game_input.h; shared/source/game_entry_manager.cpp。

### F05 · 禁止游戏代码补丁

管理器的 Patch 操作直接返回 PatchDenied；没有 VirtualProtect、跳板分配、原版函数入口或调用点改写。旧补丁登记地址仅作为历史资料。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：shared/source/game_entry_manager.cpp; assist/source/input_patch.h。

### F06 · 统一消息与外部输入调度

每个游戏仅有管理器 WH_GETMESSAGE / WH_CALLWNDPROCRET 两个 Windows 消息订阅。宿主按 4 ms 目标轮询增强鼠标输入并保存捕获时对局、边沿、按下时修饰键与取消代次；上下文超过 150 ms 未续期则停采并清除，切局/菜单/失焦重新播种按键状态；战术控制目标间隔 16 ms。实际延迟取决于系统调度和游戏消息泵。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：shared/source/host_game_input.h; assist/source/pointer_host.h。

### F07 · 统一请求、接续和事件提交

命令、选兵、外部规划、协同与增强输入使用同一 Shared 协议和同一游戏线程调度。观察到与点击目标格相符的原版 Move 状态后才追加 CS；不修改原版任务、事件字节或接收函数。请求没有独立 Hook。队列满保留未提交部分，已接收前缀不重发。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/pointer_intent.h; assist/source/pointer_bridge.h; assist/source/bridge.cpp。

### F08 · 对局身份与延迟恢复

请求携带对局代次、时间和对象版本；真正换局会取消旧请求。短暂窗口丢失、暂停或回调延迟不直接等同退出。主程序不伪造 ACK；线程迁移必须确认旧线程对象已结束。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/session_epoch.h；control_link.h；command_delivery.h；main.cpp。

### F09 · 回执与手动优先

计划、入队、观察、完成、取消和过期分别记录。每帧与跨帧仲裁避免多个自动任务争用同一单位；人工操作递增对象版本并接管任务。已入队只表示提交成功，不能冒充动作已完成。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/command_gateway.h；unit_task_arbiter.h；business_effects.h。

### F10 · 一键维修和自动维修

仅维修己方受损且未处于维修/待确认状态的建筑。默认检查 YRpp ClickRepairable (+0x157A)；勾选“允许维修不可维修的建筑”后可放宽此类型限制。收集与提交前均检查，关掉许可会跳过未提交的非标准建筑请求；已入原版队列的事件不撤回。不改原版类型数据或归属，仍通过统一 Repair 事件入口。 可通过功能单位清单限制可维修建筑类型，原有不可维修建筑许可独立生效。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp; assist/source/auto_settings_ui.h。

### F11 · 战斗单位筛选

从当前选区筛选符合战斗条件的己方单位，排除采矿或工程用途等不符合项。执行受管理器保护的本地选择 API，不向仿真队列发送攻击命令。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp 的 selectFilter。

### F12 · 空载运输选择

查找己方有运输容量且当前空载的载具，进行本地选择。它与自动装车分开：选择空车不意味着乘员已经得到进车任务。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp 的 selectFilter。

### F13 · 受损单位选择

从当前选区筛选生命值未满的己方单位，重新组成选区。它不自动修理单位，建筑维修仍走 Repair 指令。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp 的 selectFilter。

### F14 · 空闲单位定位

矿车、工程师、运输载具分别按类别检查任务和目标状态，循环定位符合条件的己方对象并移动视野。没有合法对象时返回零，不用旧指针凑结果。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp 的 findIdle；protocol.h 的 idleMission。

### F15 · 组合选兵与双按范围

多个兵种可配置同一个快捷键，注册时去重。单按合并选中屏幕内这些类型的己方单位，双按选全图；本局缺失类型跳过。宿主生成绑定本局的类型掩码，唯一 GameRuntime 在同一选兵请求里筛选全部对象，只清空一次再合并选区；无网络事件或新增 Hook。解绑某个兵种保留组内其他兵种。原有按普通武器选变体功能及其独立快捷键保留。普通功能快捷键默认拒绝冲突；勾选热键冲突时全部执行后可共享。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/type_hotkey.h; assist/source/type_bindings_ui.h; assist/source/type_select_bridge.h; assist/source/main.cpp。

### F16 · 信标清理与实验性地图揭示

地图全开默认未许可，须勾选“启用地图全开（实验性）”；宿主与 Runtime 均检查许可，关闭清除待处理请求，不回退已揭示地图。经统一管理器调用原版本地 UI 接口。恢复单机及多人地图揭示，仅作用于当前玩家；仍检查有效对局与地图数组，菜单/加载/观战不执行。它是本地显示调用，不构造模拟事件。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp。

### F17 · 自动装车分配

选中乘员后触发装车；选区有运输载具时优先限定这些载具，仅选乘员时查找附近己方载具。检查空位、乘员占位大小、载具限制和原版可进入动作，支持就近优先或兵种均衡，半径 1 至 512 格。 装车乘员和载具可分别配置类型清单，仍要求原版进入合法性、容量与体积允许。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/autoload.h；autoload_bridge.h 的 autoLoad。

### F18 · 装车预约与完成判断

已分配乘员占用预约空位，向统一 nativeMission 提交 Enter，避免每次扫描重复分配。根据任务、目的载具和实际 Transporter 判断途中、完成或人工接管；超时和失效释放预约。已测试单机及双端模式 3/4 的实际 DLL 事件提交，尚未验证完整寻路和进车动画。 重试前复核当前类型清单；清单撤销不会撤回已经进入原版队列的 Enter。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/autoload.h 的 Ledger；autoload_bridge.h 的 finishLoadingReservation。

### F19 · 智能卸载

对选中、可卸载的己方载具发送原版卸载任务并追踪乘员。乘员离车后按稳定位置和类型决定后续行为，提交失败不会当作任务已经完成。 可配置允许卸载的载具类型；乘员清单控制后续自动任务，不改变原版整车卸载行为。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/native_orders_bridge.h 的 smartUnload 与 smartUnloadTick；unload_policy.h。

### F20 · 乘员下车后的行动

根据乘员可部署、工程师、间谍、C4 等属性及原版目标合法性决定休眠、部署、进驻或攻击。目标失效、人工操作、时限或归属改变会结束跟进；不是无条件自动占领任意建筑。 可配置允许执行下车后续任务的乘员类型。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/native_orders.h；native_orders_bridge.h 的卸载状态机。

### F21 · 逃生助手

仅跟踪选中的己方载具，取消选中且载具仍存活时停止跟踪。载具被毁后，对已观察到的存活乘员继续跟进。可部署乘员保留原版散开移动，散开跟踪窗口为 180 帧；每名乘员只要原版 CanDeploy 允许就在当前调度提交 Deploy，不等待同格其他乘员，也没有固定停稳两帧延迟。条件不满足时不发 Sleep 阻止散开。显式停止、取消、正常下车、归属变化或超时结束跟进；不创造幸存者。仅保留一个逃生开关。 载具与乘员类型分别可配置。仍然只登记当前选中的己方载具；撤销类型后停止后续跟踪。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/native_orders_bridge.h; assist/source/unload_policy.h。

### F22 · 自动 CS：原版移动后接续

保留右键原版移动。宿主捕获右键释放时的输入意图，Runtime 等待己方选中单位的 Move 与相同目的格可观察后，再经 nativeMission 追加一次 AttackMove。检查单位类型白名单、武器、对局、对象身份和新输入代次。没有改写原版 QueueMegaMission；不改鼠标谓词。

实现路径：宿主输入意图 → 原版 Move 观察（右键）/主动命令（中键）→ 统一事件入口。

源码：assist/source/pointer_host.h; assist/source/pointer_bridge.h; assist/source/auto_cs_bridge.h。

### F23 · 按键 CS

按住配置键配合独立配置的 CS 鼠标键（默认中键），留空则仅鼠标。配置为右键时接续原版 Move；直接攻击移动使用另一独立按钮（默认侧键 1）。Shift 右键规划不接续；聊天、菜单、失去前台或过期时拒绝。原版光标不再被强制改成 CS 光标。

实现路径：宿主输入意图 → 原版 Move 观察（右键）/主动命令（中键）→ 统一事件入口。

源码：assist/source/pointer_host.h; assist/source/pointer_bridge.h。

### F24 · 高级移动攻击

按住配置键配合本功能鼠标键（默认中键，均可配置）；右键模式确认原版 Move 后追加初始 AttackMove，再登记外部规划任务。后续目标选择仍在 EXE，回到统一管理器验证并提交。 高级移动攻击源单位类型可独立配置；捕获与提交旧计划前均复核清单。

实现路径：宿主输入意图 → 原版 Move 观察（右键）/主动命令（中键）→ 统一事件入口。

源码：assist/source/native_orders_bridge.h; assist/source/external_planner.h; assist/source/pointer_bridge.h。

### F25 · 磁电轮换攻击

配合独立磁电鼠标键（默认中键）；配置为右键时：确认原版 Move 后仅登记任务，不再替换原版移动。中键方式先发普通 Move 再登记。外部 Planner 读取出手/重装填证据轮换目标，每次攻击通过统一原版事件通道。没有武器选择或射击函数 Hook。 磁电源单位类型可配置；仍要求原版磁力武器和 Locomotor 弹头，清单不赋予新武器。

实现路径：宿主输入意图 → 原版 Move 观察（右键）/主动命令（中键）→ 统一事件入口。

源码：assist/source/native_orders_bridge.h; assist/source/external_planner.h; assist/source/magnetron_cycle.h。

### F26 · 新输入取消与手动优先

宿主观察右键按下以及 S/D/X 边沿，取消旧的未提交增强批次；有效取消请求按捕获的己方选区撤销工具任务。普通事件仍由游戏处理。状态观测继续识别原版任务变化。输入队列上限 16，超时不补发；短于轮询间隔的点击可能漏采，不会补造点击。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：shared/source/host_game_input.h; assist/source/pointer_bridge.h; assist/source/object_index.h。

### F27 · 满载返矿与到场优先

矿车/矿场支持独立自定义 ID 清单；不勾选自动识别，勾选后仅使用清单，空清单排除全部。矿车须有储矿容量，目标须在该矿车原有 Dock 清单内。设置即时生效与自动保存，提交前重新检查。满载己方矿车由外部 Planner 选择兼容矿场并排队。2格内已到场车可优先于4格外预约车：经统一 OreYield 原生 Move 让位，观察原版 Move 和预约释放后再 Enter。每矿场一轮一车、每载货任务最多被插一次，被插车保留下一轮优先。最多3次让位重试，未释放180帧退出；本轮360帧上限。不打断到场/卸货或手动接管，安全格不足时保留原排队。Runtime 复核原版状态后统一入单机 DoList / 联机 OutList，不直接写 radio。150ms/6帧及对局/手动版本检查保留。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/external_planner.h; ore_queue.h; external_orders_bridge.h; main.cpp。

### F28 · 矿场排队与等待格

根据矿厂占用、无线电关联和周围格占用建立排队任务，先移动到等待格，再在条件允许时进入矿厂。预约会随对象失效、人工接管或对局切换清理，不直接修改矿厂生产或玩家资金。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/ore_queue.h；external_planner.h 的 waitingCell 与 accepted。

### F29 · 卸矿结果观察

通过矿量和任务状态变化统计观察到的卸货与未确认次数；数字是只读观察证据，不是游戏真实收入流水。玩家给矿车新命令时自动规划让出控制。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/business_effects.h；external_planner.h；native_input_hooks.h。

### F30 · 高级进驻建筑与载具

按住进驻配置键（默认 Alt）并按本功能鼠标键（默认中键）指向目标，捕获最多 512 个源对象，向现有外部协同分配器提交。按同类目标容量分配，Runtime 复核原版进入合法性后发送 Enter/Capture。超限拒绝增强请求，普通左右键仍原样运行。 源乘员和目标建筑/载具均可配置类型；同类目标分配、容量和原版进入判断保留。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/pointer_bridge.h; assist/source/cooperative_bridge.h; assist/source/extra_settings_io.h。

### F31 · 飞机高级攻击

按住对应攻击配置键并按本模式鼠标键（默认中键）指向敌方目标。目标分配、受击上限仍由外部协同规划器完成，GameRuntime 复核对象与武器兼容并构造 Attack 事件。不再拦截 FootClickedAction。 飞机源单位类型可配置；仍须是原版飞机或符合原有空中状态的载具，并具备兼容武器。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/cooperative_bridge.h; assist/source/extra_settings_io.h。

### F32 · 目标类型配额

按类型配置第一/第二次和最大分配数量；有效输入自动生效。取消类型覆盖时直接删除覆盖并恢复默认显示，不再先验证即将废弃的数字框。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/additional_settings.h；extra_settings_io.h；auto_settings_ui.h。

### F33 · 飞碟主武器

开启后 配置键（默认 Ctrl，可留空）+本功能鼠标键（默认中键）指向建筑，识别飞碟/吸取副武器类型并提交原版对地强制攻击事件。原版正常点击与武器选择保持原样；不直接修改武器、任务或目标。 飞碟源单位类型可配置；保留飞碟 ID 或原版吸取副武器识别，不赋予普通载具新能力。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/cooperative_bridge.h。

### F34 · 变卖受吸取的己方建筑

开启后 配置键（默认 Ctrl，可留空）+本功能鼠标键（默认侧键 2）指向己方受吸取建筑，经统一对象事件构造器提交 Sell。目标必须仍存活、归自己所有。无变卖模式/光标 Hook。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/cooperative_bridge.h; assist/source/pointer_bridge.h。

### F35 · 变卖己方载具

开启后 配置键（默认 Ctrl，可留空）+本功能鼠标键（默认侧键 2）指向载具。保留原版接收端条件：目标归发送玩家、载具格子中存在建筑。构造 Sell 对象事件，目标标签为 0x34。不会伪造其他玩家身份、改变格子链表或直接调用出售执行函数。模型执行了用户 EXE 的 Sell 接收分支，最终出售/经济效果仍待游戏实测。 可配置允许出售的己方载具类型；仍须满足原版所在格有建筑等出售条件。

实现路径：中键/侧键意图 → GameRuntime 查询/复核 → 原有规划或统一事件入口。

源码：assist/source/cooperative_bridge.h; assist/source/pointer_bridge.h。

### F36 · 变卖敌方建筑：停用

不再伪造敌方 House 发出 Sell。配置加载强制关闭、界面禁用、Runtime 再检查己方归属。

实现路径：停用：不提供原版规则/身份绕过。

源码：assist/source/extra_settings_io.h; assist/source/extra_controls.h; assist/source/cooperative_bridge.h。

### F37 · 随处建造：停用

该能力需要绕过原版放置合法性判断，普通移动/生产事件无法等价实现。删除 0x464AC0 拦截，旧配置不能恢复，Runtime 返回不支持。

实现路径：停用：不提供原版规则/身份绕过。

源码：assist/source/placement_bridge.h。

### F38 · 科技清单与离线类型缓存

缓存已见类型的 ID 和名称，支持搜索、手动添加 ID、预设和自定义启用清单。离线缓存用于配置，不代表当前对局存在该单位；执行时必须重新映射到当前索引。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：assist/source/type_catalog.h；tech_profile.h；main.cpp 的 fillTech。

### F39 · 科技全开：生产连续性待修复

补图实验不能防止原版 Abandon/AbandonAll 取消生产，已撤出。删除两个原版侧栏 CanBuild 调用点改写，不补造无资格图标，不绕过前置科技。类型目录和条件解析保留。界面禁用科技全开，旧配置不能恢复。

实现路径：停用：不提供原版规则/身份绕过。

源码：assist/source/sidebar.h; assist/source/main.cpp。

### F40 · 合法生产与生产观测

外部生产仅限单机，提交前调用正常 CanBuild（limitOnly=false）并检查工厂；原生侧栏不被改写。生产完成/暂停观测以及本地图标闪烁保留。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp; assist/source/sidebar.h。

### F41 · 超级武器快捷键

超武绑定按对象 ID 持久化；列表刷新、换序和项目消失后按 ID 恢复选择并同步按键框，清空列表时清空按键显示。下拉展开期间使用可见列表的 ID，避免把旧行绑定到新快照的另一超武。原版选择超武操作仍由统一入口授权。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：assist/source/bridge.cpp 的 superSelect 与 namedSuper；main.cpp 的 bindSuper。

### F42 · 规则前提分析工具

tools 下的脚本读取规则文件，分析 Prerequisite、拥有方和科技条件并输出审查结果。它是离线只读工具，不是游戏内“解锁全部科技”的实现。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：tools/tech_prerequisite_audit.py。

### F43 · 载员与剩余容量

按统一只读显示策略随菜单隐藏/恢复。可见载具同一逻辑帧内重读坐标与乘员链，并复核对象身份/类型/所有者及存活状态；插值采用外部采样帧，容量立即更新。前台目标跟随 5–500 Hz 配置，实际耗时见 Runtime.log，不承诺实测屏幕帧率。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/display_state.h; tactical_sampler.h; assist/source/label_motion.h; main.cpp。

### F44 · 铁幕与力场倒计时

读取防护相关计时，按模拟帧计算剩余时间，绘制在对象附近。暂停时剩余模拟时间不会按墙钟强行扣减；标签位置可以独立按显示时钟平滑。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：assist/source/protocol.h 的 remaining；label_motion.h；tactical_sampler.h。

### F45 · 冻结倒计时

读取超时空相关冻结信息，启用后附加剩余帧提示。它反映当前采样状态，不修改对象解冻时间。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/tactical_sampler.h；assist/source/label_motion.h。

### F46 · 心灵探测和攻击指向

按采样到的目标关系显示指向线，读取失败或目标消失后不沿用无效对象。显示层不会因此发出攻击指令。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/tactical_sampler.h；assist/source/main.cpp 的 paint。

### F47 · 箱子位置与预览

显示箱子所在格；有预设结果时显示类别，随机箱显示为随机。当前没有启用可靠的开箱结果 hook，不能把预览当作已经获得的奖励；历史 CrateResults 协议字段保留兼容但不是当前启用功能。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：external_reader/source/tactical_sampler.h；assist/source/crate_names.h；main.cpp 的 save。

### F48 · 文字提示与刷新

显示任务反馈、装车统计和生产变化，可设置提示时长、上边距、刷新率及耗时信息。文字和头顶标签共用战术浮层，但与单位统计浮层独立；显示由 renderStatus 决定，命令状态不再一票否决有效显示。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：assist/source/main.cpp 的 paint、mergeExternal 与 clockState；frame_clock.h。

### F49 · 玩家与单位数量

读取有效玩家、常规类型及拥有数量，区分本机与其他玩家，可选是否显示建筑。过滤旁观者和退出等不适合展示的玩家，列顺序优先本机，退场后压缩空列。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：unit_monitor/source/sampler.h；owned_counter.h；main.cpp。

### F50 · 生产队列与进度

按工厂对象读取排队数量、当前生产、进度和暂停状态，避免把多个建筑指向同一队列当作重复生产。显示拥有数量和新增生产，而非简单累加界面图标。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：unit_monitor/source/sampler.h 的 sample；unit_monitor/source/core.h。

### F51 · 图标持久缓存与绘制缓存

外部后台线程将实际读取到的图标/色盘保存到 cache/unit-icons，按游戏路径和资源指纹隔离；小资源校验内容，大型 MIX 等资源以大小与更新时间识别。规则、地图、资源或程序变化时换用新的缓存。记录按类型 ID、图标文件名和类别匹配，校验版本、长度、条数与校验和，损坏缓存忽略；临时文件写完后原子替换。新对局可复用磁盘图标，实时图标可读后校正；首次从未读取过的图标仍需等游戏载入。GDI 位图保留，图标或色盘变化才重建。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：unit_monitor/source/icon_cache.h; sampler.h; main.cpp。

### F52 · Tab 显示与快捷键

删除单位统计自己的 WH_KEYBOARD_LL。改为现有 UI 时钟轮询 Tab，不吞原版 Tab，游戏原来的 Tab 行为会同时保留。Ctrl+Alt+H 和显示按钮可独立显隐；勾选常驻显示可完全不使用 Tab。失焦/菜单/读取无效仍隐藏，面板与日志说明原因。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：unit_monitor/source/main.cpp。

### F53 · 演示和重新连接

布局演示明确使用模拟玩家、数量和图标，用于查看排版；重新连接重建当前读取对象。统计暂停不会释放战术模块仍在使用的公共读取句柄。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：unit_monitor/source/main.cpp 的 demoData、attach 与 detach；shared_reader.h。

### F54 · 光标游戏识别与资源采集

识别兼容 Yuri 进程并取得鼠标对象、原始帧和色盘，保留资源缓存。当前光标连接要求明确的单个游戏进程；资源不匹配时报告失败，不把不明 SHA 当作标准布局。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的 attach 与 capture；recognition.h；resource_capture.h。

### F55 · 原版光标隐藏/恢复

通过统一 GameRuntime 在绑定游戏线程调用原版鼠标对象 Hide/Show，跟踪配对引用次数，失败不写回陌生对象；Windows 光标也由同一管理器资源路径处理。这里改变本地显示，不改游戏代码或仿真任务。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：cursor/source/hide.cpp。

### F56 · 外置高刷光标

透明、点击穿透的覆盖窗口跟随系统坐标显示光标，目标刷新率可自动或设为 30 至 500。帧变化与位置变化分开处理，缓存减少重复创建；高刷新率不提高游戏模拟帧率。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的运行循环；native_cache.h；perf_policy.h。

### F57 · 游戏内光标刷新

外部宿主经唯一 host_game_input 校验窗口与前台后发送 WM_SETCURSOR；等待有界且不持调度锁。游戏线程上的光标资源发布仍由 GameRuntime 消费。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：shared/source/host_game_input.h; cursor/source/hide.cpp。

### F58 · 缩放和热点修正

支持缩放比例、自动缩放、保持纵横比、手动热点、游戏/系统分辨率和 DPI 模式。原生与超武热点有单独修正，显示坐标与点击位置应通过用户实际分辨率验证。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/scale.h；ui_dpi.h；native_hotspot.h；super_hotspot.h；live_settings_ui.h。

### F59 · 状态面板与诊断

可打开刷新、缓存、资源采样和时序信息；使用深色浅字，并统一发布版本显示。此面板用于光标诊断，不是战术文字播报。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的 hudProc 与 updateHUD。

### F60 · 平滑补帧与自适应时序

在识别出的动画分组内做连续帧过渡，结合实际换帧节奏推断播放相位；跨组、非连续帧或不可靠采样会重置。不能对任意帧跳转盲目插值。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/motion.h；adaptive.h；playback.h；stream_inference.h。

### F61 · 离线预处理和缓存

光标包可离线预处理每段 1000 等分的动画，运行时优先播放已经完成的分段。预处理进程低优先级运行，可取消并保留已完成段；损坏、尺寸或散列不匹配缓存被拒绝。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：engine/precompute.py；engine/dis_render.py；cursor/source/precomputed.h；persist.h。

### F62 · 图案导入和切换

导入符合 450 帧、55×43 标准布局的 SHA/SHP 及配套色盘，保存为独立光标包；切换图案自动保存，动画分组和预览属于该包。图案切换不会覆盖游戏目录中的原资源。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的 chooseStyle 与导入路径；engine/preview_builder.py。

### F63 · Shift5

修复设置处理器检查已删除 clickHook 句柄而误关并保存连点的问题，勾选状态直接决定启用。按键观察在低显示帧率下仍按最多 4 ms 目标间隔运行，复用原有计时器，高帧率绘制不降速。旧 INI 的关闭值保留，受影响者重新勾选即可恢复，避免替用户强行打开。自动模式检测到 Ares 时交由原版；无法确定时仍跳过，必要时用户可选强制补点。删除光标模块自己的 WH_MOUSE_LL。通过现有宿主循环观察物理左键边沿，检查 Shift、区域及 Ares 原生支持；额外点击仍由唯一 host_game_input::clickPair 发送且配平抬起。轮询可能漏掉极短点击，不拦截原版点击。

实现路径：统一管理器原版调用或事件入口；无功能级 Hook。

源码：cursor/source/main.cpp; shared/source/host_game_input.h。

### F64 · 透明 SHA 安装与恢复

这是明确修改游戏文件的可选操作，必须退出游戏。安装记录原文件是否存在并备份，遇到既有备份不覆盖；恢复按记录还原原文件或移除工具创建的文件。手动放置的透明 SHA 不冒充工具拥有的备份。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的 install 与 restoreFile。

### F65 · 自动检测和界面快捷键

可自动等待游戏、暂停或恢复光标，切到高级设置不停止已运行功能。Ctrl+Alt+F10 控制面板，F11 开关，F12 退出；设置自动应用，输入尚未完整时保留最近有效值。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：cursor/source/main.cpp 的 autoTick、togglePrimary 和主消息循环；live_settings_ui.h。

### F66 · 自动保存与配置隔离

有效设置自动应用和保存。光标设置逐字段校验，宽高作为关联对提交；一个无效数字不再阻塞其他开关。写盘失败后再次应用会重试写盘，不会因运行值未变而错误显示已保存。战术设置沿用磁盘防抖，退出前刷新；单位统计设置即改即存。 即时生效与自动保存逻辑保持；删除重复的静态自动保存标签，使用顶部统一说明。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：assist/source/auto_settings_ui.h；cursor/source/main.cpp；unit_monitor/source/main.cpp。

### F67 · 一页控制台与当前状态

快捷键字段读取当前 HKM_GETHOTKEY 自绘，保留原生输入/通知；首绘和失焦也显示值。光标主页及高级设置改用公共按钮、开关与配色，固定展开宽度。沿用即时保存与一页滚动，旧帧入口只显示统一消息模式。

实现路径：读取/外部展示/配置资源；无游戏函数拦截。

源码：assist/source/main.cpp; assist/source/extra_controls.h; shared/source/modern_ui.h。

### F68 · 各功能单位类型清单

统一配置 14 项独立类型清单：飞机、飞碟、磁电、高级移动、装车乘员/载具、进驻乘员/目标、卸载载具/乘员后续任务、逃生载具/乘员、变卖战车、维修建筑。面板选择功能后编辑 ID 列表；每项自定义开关独立。未勾选保留原有识别；勾选且清单为空不处理任何类型。最多 512 项、每项 23 个 ASCII 字符，支持大小写归一、去重与中文分隔符。离线编辑、即时生效、自动保存；配置半写或损坏拒绝发令。只筛选类型，不写原版规则，不绕过所有权/武器/容量/进入合法性。矿车、矿场、自动 CS 与组合选兵沿用各自配置。

实现路径：EXE 配置 → Shared 版本化快照 → 唯一 GameRuntime 在捕获与提交前复核。

源码：assist/source/feature_type_config.h; feature_type_bridge.h; auto_settings_ui.h; type_bindings_ui.h; extra_settings_io.h。

### F69 · 敌方位置标记

显示设置中的“敌方位置标记（包含未揭示区域）”默认关闭，修改即时生效并自动保存。复用统一读取器已有对象遍历，按当前玩家的敌我关系筛选存活单位和建筑，不检查地图是否揭示；排除己方、盟友、中立及已进入运输工具的乘员。当前战场视口内按玩家颜色绘制位置符号与 类型名称，建筑方框、其他单位菱形；不绘制到侧栏。跟随覆盖层现有 FPS 设置，使用同一采样时钟、Motion 插值和绘制窗口，同一逻辑帧内继续更新坐标。类型名称及文字位图缓存，最多 2048 个标记；菜单、失焦、跨局、读取失效或数据过期时隐藏，发布前复核当前帧、玩家、镜头和显示状态。仅显示游戏内存已有且布局可识别的对象，不创造未加载的数据。不改变战争迷雾、不调用地图揭示、不写游戏状态、不提交事件或新增 Hook。

实现路径：统一 ReadSession → TacticalSampler → 现有 Motion 与覆盖层；宿主只读展示，不进入 GameRuntime 命令通道。

源码：external_reader/source/enemy_markers.h; external_reader/source/tactical_sampler.h; assist/source/main.cpp; assist/source/extra_controls.h; assist/source/extra_settings_io.h。

### F70 · 热键冲突时全部执行

战术面板新增独立开关，默认关闭，即时生效并自动保存到 [Hotkeys] ExecuteAllMatches。开启后，同一键盘组合只注册一次 Windows 热键，再按现有顺序将全部匹配动作放入宿主队列，逐项等待命令/选兵回执，避免后写覆盖；组合选兵仍按类型合并，两种双按状态独立。增强鼠标按每项按钮、按住键和功能许可收集全部匹配项，最多十项；全部绑定同一次捕获的来源时间、选区、目标、对局和取消代次，经同一指针邮箱依次提交。前一协同请求被规划器消费且回执完成后才切换模式，不覆盖未处理请求。不改写原版点击或事件，不绕过每帧额度与原版合法性；同一单位的后续命令可能覆盖前项，持续规划仍只保留最后一项。键盘待发队列上限128、时效2500ms；鼠标首项150ms内接收，已接收组合的后续项共用原有1500ms/90帧总期限，不刷新捕获时间。新操作、菜单、失焦、跨局或配置变化取消未提交鼠标项。关闭开关恢复原有单项规则，保存的重复绑定不被改写。其他程序占用的系统热键仍可能注册失败，面板会报告。Assist 协议42要求整包更新。

实现路径：一次宿主捕获 → 有界队列 → 现有命令/选兵/指针邮箱 → 单一 GameRuntime 与原版构造器；无新增 Hook。

源码：assist/source/hotkey_routes.h; hotkey_host.h; pointer_intent.h; pointer_batch.h; pointer_host.h; pointer_bridge.h; auto_settings_ui.h; extra_settings_io.h。

## 验证和已知边界

模型使用本次编译的 x86 DLL 与实际原版事件构造器，世界、Win32、传输和接收周边有桩实现；双端模型按帧检查事件传输与状态摘要，不是两个完整游戏实例，也没有真实寻路、战斗和经济模拟。详见 [测试报告](TEST-REPORT.html) 及 verification 中与本 DLL SHA256 绑定的结果。

本版尚未完成 Windows 游戏实机、LBW、平台双人联机、屏幕捕获和真实延迟/帧率测试。1.0.2 是版本编号，不作为所有平台已验证的承诺。科技全开、随处建造和敌方建筑变卖仍停用；地图揭示需主动勾选实验性许可。
