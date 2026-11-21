# PR #71 功能采样审计

审计基线为 [PR #71](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/pull/71) 的 [937d91d8](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/937d91d844ebf2e1a6f1a74cb3623edd40119dad)。覆盖常规控制、自动化、显示与监控、预定、光标和直接操作原生界面的模式。这里记录源码调用路径及合成测试结果。

E07 的全局提前返回已在后续修复。缺严格索引时继续更新局部预览、发布带原始帧号和时间的候选、准备维修与找矿计算。依赖同帧名单的准入、原生提交和所有权清理等待严格索引。同一模拟帧可先发布观察结果，再处理随后就绪的命令输入。 [派发](../assist/source/bridge.cpp) · [候选规划发布](../assist/source/external_orders_bridge.h)

## 采样与执行边界

- 生产环境由 Runtime 的 `CommandCaptureWorker` 读取世界、建立 UID 索引并发布不可变样本。Host 经共享内存取得同源样本。旧同步全图、Host VisualCache、补采旁路和影子采样器仅在显式测试配置保留。页需求受上限约束，提交和取得结果走非阻塞尝试。首启仍有游戏回调内创建事件和线程的成本，见 E07。 [架构原则](2026-11-20-runtime-single-pass-sampling-principles.md) · [Runtime 交接](../assist/source/world_frame_runtime.h#L254) · [采集线程](../shared/source/world_frame_command_worker_win.h#L238)
- Runtime 的 `read/get/vec` 在 `SampleReadScope` 或 `ObservationReadScope` 内从封口样本取值，作用域外默认读实时内存。Host 的 `read/readFresh` 也需要先绑定 `WorldFrameScope` 或 `FrameScope` 才会读样本。一路使用 `SharedReadView` 的类型名不能证明已绑定样本。显式 `readLive` 仍读现场。 [Runtime 读接口](../assist/source/bridge.cpp#L176) · [Runtime 采样作用域](../assist/source/world_frame_runtime.h#L185) · [Host 读接口](../external_reader/source/process_reader.h#L231)
- 严格采样作用域要求同局同游戏帧。观测作用域可读取当前世界的近期候选，用于发现或预测，Runtime 这条路径的候选时限为 280 ms。按对象核验 UID、归属、存活、任务、人工操作版本、目标或格子，并调用要求在游戏线程运行的原生接口，属于提交或确认。实时读取整组候选、类型目录、载员或战术区域属于发现。完整性失败保持未知，命令准入和生命周期推进继续遵循各入口的同帧、时效和现场证明。 [候选条件](../assist/source/world_frame_runtime.h#L154) · [命令验证](../assist/source/execution_proof.h)

## 控制与自动功能

| 功能 | 候选事实与计算位置 | 提交、缺样及例外 |
| --- | --- | --- |
| 普通选择过滤、选闲置单位、同类选择、飞机闲置选择 | Runtime 使用严格样本、Worker roster 或 Host 据样本发布的飞机清单。 | 实时校验选中资格和对象身份。无严格 roster 时返回待重试状态。变体键选同类暂挂，返回 `-12`。 [选择](../assist/source/bridge.cpp#L980) · [闲置飞机](../assist/source/bridge.cpp#L1130) · [同类及变体](../assist/source/type_select_bridge.h#L15) |
| 高级移动攻击、磁电轮换、全局吸与摔落 | 自动规划的 Actor/目标由同一 Runtime 样本投影，默认重规划在 Host。Runtime 的全局载具候选来自 Worker 所建索引并分批取用。 | Runtime 做原生武器合法性及逐对象提交证明。原生战斗判定和候选投影仍有回调成本，见 E08。 [规划世界](../assist/source/external_orders_bridge.h#L74) · [战斗候选](../assist/source/combat_probe_bridge.h#L86) · [原生工作循环](../assist/source/native_orders_bridge.h#L544) |
| 防偷／防御优先攻击 | 建筑存在性使用严格样本，对来源序号与配置版本做证明。目标规划用统一规划世界，默认在 Host 计算。 | 建筑名单缺页时不把未知当作没有建筑。Runtime 核验对象后提交。 [建筑存在性](../assist/source/anti_theft_sample_bridge.h#L20) · [规划](../assist/source/external_orders_bridge.h#L84) |
| 高级进驻、飞机分配攻击 | 协作输入用观测样本，进驻占位核对用严格样本。默认 Host 计算配对。 | 提交由 Runtime 校验来源、参与单位、目标及容量。拿不到完整协作输入时不发布新计划。 [输入](../assist/source/cooperative_bridge.h#L11) · [容量](../assist/source/cooperative_bridge.h#L155) · [提交](../assist/source/cooperative_bridge.h#L260) |
| 自动装车 | 单位和载具候选、载量、预约原始观测来自样本。默认 Runtime 做配对。 | 原生可进驻询问、活体确认及入队仍在游戏线程，涉及候选对的询问有回调成本。输入缺页时等待，最终确认用严格样本及现场资格。 [输入与配对](../assist/source/autoload_bridge.h#L156) · [提交](../assist/source/autoload_bridge.h#L87) |
| 普通 Ctrl+D 智能卸载 | 后续指令的敌我候选使用统一样本。Ctrl+D 之前的乘员历史、触发时的乘员登记和队列准入仍直接扫步兵表。 | 整批准入有身份、人工版本与未知重试。独立取数仍违反单采样，见 E01。 [载员记录与准入](../assist/source/native_orders_bridge.h#L111) · [热键与队列](../assist/source/native_orders_bridge.h#L295) · [续令计算](../assist/source/unload_compute_bridge.h#L81) |
| 载员自动逃生 | `watchEjections` 使用严格样本的步兵名单，整次读取完整后提交发现结果。续令候选同源，默认 Runtime 计算。 | 对已经登记的载员和载具做候选身份、落地、任务和目标的实时核验。与 Ctrl+D 共用部分执行代码，初次发现入口有区别。 [逃生观察](../assist/source/native_orders_bridge.h#L368) · [续令](../assist/source/unload_compute_bridge.h#L93) |
| 手动一键维修、自动维修与不可维修建筑选项 | 建筑、血量、上限和维修状态来自观测样本。当前生产策略默认在游戏回调遍历建筑候选并计算维修 ID。 | 缺页保留重试。对待处理 ID 逐个验明身份、修理状态和现场合法性后提交，见 E08。 [维修候选](../assist/source/bridge.cpp#L1178) · [维修执行](../assist/source/bridge.cpp#L1431) · [生产计算分配](../assist/source/compute_policy.h#L4) |
| 自动倒矿、倒完回采矿、恢复暂停矿车 | 外部规划用统一规划世界。Runtime 的卸矿历史、矿厂匹配、矿格搜索也读取观测样本。矿厂名单在 `oreHarvestObserve` 的外层采样作用域内。 | 搜索分批，预算最多 2048 格/轮，单任务步进最多 256 格。缺页不推进搜索检查点。确切执行前复读矿车和目标格。 [卸矿观察](../assist/source/ore_harvest_bridge.h#L156) · [找矿与校验](../assist/source/ore_harvest_bridge.h#L222) |
| 矿车防蜘蛛与恢复原工作 | 矿车、候选威胁与矿厂索引使用统一规划世界及严格样本。退场确认使用 Worker 发布的同帧 UID 索引。 | 实时证据限于目标个体身份与明确死亡状态。Worker 名单不可用时保留原任务。 [矿厂索引](../assist/source/harvester_defense_bridge.h#L136) · [退场证明](../assist/source/harvester_defense_bridge.h#L333) |
| 被绑弹步兵自动找载具 | 严格样本读取候选步兵和运载目标，完整后才提交候选。 | 原生进驻合法性及本次配对身份在 Runtime 核验，缺样时留待后续。 [收集与提交](../assist/source/bomb_board_bridge.h#L38) |
| 自动躲弹、倒车躲弹、防空导弹拦截 | 弹体、单位、反向躲弹地形格和拦截候选来自严格/观测样本。默认由 Runtime 计算。 | 逐候选验证实时对象、轨迹、人工任务，部分原生武器查询仍在回调。不会按缺失名单取消既有租约。 [躲弹](../assist/source/dodge_bridge.h#L198) · [倒车](../assist/source/reverse_dodge_bridge.h#L76) · [拦截](../assist/source/missile_bridge.h#L274) |
| 阵型、快速阵型、模糊攻击、区域分配、自定义巡逻 | Host 接收手势预览数据。Runtime 从当前选区、格子与当地对象读取手势事实。模糊和区域下令仍实时枚举格和格内对象，见 E10。 | 武器/进驻询问与最终提交在游戏线程。起手时严格索引不可用，部分消息入口直接拒绝。 [阵型](../assist/source/formation_bridge.h#L60) · [区域候选](../assist/source/mo_orders_bridge.h#L119) · [手势准入](../assist/source/mo_orders_bridge.h#L299) |
| 飞机路径点、点击放置与进驻、攻击脚下／保持任务 | Host 飞机路径推进使用封口样本观察位置和任务。点击、手势起手、选区取证和现场命中含实时读。 | Runtime 提交前查对象和目的地。Host 对已跟踪的飞机另外确认身份与退场。实时选区和现场投影成本见 E06。 [Host 路径](../assist/source/air_path_host.h#L55) · [Runtime 路径](../assist/source/air_path_events_bridge.h#L27) · [点击](../assist/source/pointer_host.h#L91) · [攻击脚下](../assist/source/attack_ground_bridge.h#L43) |
| 控制权模式、变卖、摧毁和切换玩家 | 这些入口处理当前对象、选区、House 名单、控制所有权及原生权限条件。 | 属于输入和执行维护。活跃对象及 House 的定点读留在所需线程。本项按采样审查，锁槽和联机协议兼容性属于另一次验证。 [控制参数](../assist/source/tech_ui_breakpoint_bridge.h#L224) · [玩家状态](../assist/source/player_test_bridge.h#L167) |
| 手动 CS、取消自动任务、键鼠和聊天门控 | 显式输入和当前对象或选区，配合已有样本索引。 | 自动 CS、步兵强制进驻入口和原生场景高刷当前由生产开关关闭。强制刷弹在游戏 tick 中明确重置。编辑或保存旧设置不使相应入口生效。 [开关](../shared/source/feature_availability.h#L2) · [准入](../assist/source/disabled_feature_policy.h) · [定时任务](../assist/source/bridge.cpp#L1877) |
| 超武选择、持续授予、科技全开和科技加强 | 超武选择/授予直接查本地玩家超武表。科技功能有目录维护、原生建造图标盘点和刷新。 | 当前对象的选取与写入前校验保留。周期盘点和目录候选读取单独列为 E03、E05。 [超武选择](../assist/source/bridge.cpp#L1255) · [授予](../assist/source/superweapon_grant_bridge.h#L30) · [侧栏维护](../assist/source/sidebar.h#L148) |
| 输入解锁、阻止触发切屏、单机 FPS、视角缩放与小图限位、低伤过滤、原生放置和镜头定位 | 这些运行期入口消耗当前输入、局部原生状态、已解析对象或坐标。 | 按原生操作或渲染线程约束执行。建造放置仍含现场格子/产品核对，原生副画面也存在独立的选区固定和游戏内绘制动作。 [调度](../assist/source/bridge.cpp#L1832) · [放置](../assist/source/native_placement_preview_bridge.h#L106) · [原生副画面](../assist/source/native_pip_bridge.h#L79) |

## 显示、统计与预定

| 功能 | 数据源 | 现场读取与缺样 |
| --- | --- | --- |
| 信息覆盖层、战术单位面板、载员气泡、装甲和心灵状态、箱子、敌方及特殊单位标识、超武就绪及生产提醒 | 主 Host 的 `sampleExternal` 从同一 Runtime 样本取信息。Host 的 `tactical_sampler` 仍有全载具容量、铁幕和心灵等投影，通过采样作用域读封口数据。 | 没有样本时等待，并校验保留数据的局、连接及来源。Host 实时选区指纹用于界面去重，见 E06。Runtime 自己的备用覆盖层见 E05。 [Host 样本](../assist/source/main.cpp#L1548) · [状态投影](../external_reader/source/tactical_sampler.h#L330) |
| 主屏高刷、地形单位和特效、实景小地图 | 主渲染和小地图锁定场景的同一来源 token。地形、单位姿态、弹体、图层和特效的世界数据走场景样本，资源解码在 Host。 | 缺必要页时等待完整输出。实时窗口、视口、地图几何或摄像机确认用于呈现和防止换局串帧，未绘制时不会改回独立全图采集。 [主渲染](../assist/source/render_upgrade_host.h#L124) · [小地图](../assist/source/minimap_overlay.h#L42) · [场景共享](../external_reader/source/shared_scene.h#L88) |
| 第二摄像头、单位跟踪、监控窗跳转 | 独立窗口和场景线程消耗统一样本。跟踪查找和导航的单位搜索绑定 Resources/Passengers 样本。 | 当前框选与目标 UID/归属复核使用现场数据。场景、跟踪和输入各有自己的新鲜度判断。 [跟踪](../assist/source/watch_host.h#L78) · [场景](../assist/source/watch_scene.h#L44) · [跳转](../assist/source/watch_windows.h#L64) |
| 预定面板、预定建筑及步兵/战车、未解锁科技预测 | 候选列表、建造科技预测和相关目录通过 `Resources` 作用域。生产、队列和原生侧栏在服务和点击时仍有独立现场观察。 | 不完整预测保留原队列，预估可用不等同原生许可。点按命中、工厂身份、产品、容量、原生侧栏出现/移除用实时数据。持续轮询见 E09。 [候选和预测](../assist/source/construction_host.h#L105) · [重新判定](../assist/source/construction_host.h#L232) · [服务](../assist/source/construction_host.h#L452) |
| 侧栏矿车／电力、原生 UI 层、渲染反馈和阵型预览 | 矿车名单、原生 UI 世界投影和阵型/地形样本绑定统一来源。 | 当前视口、局部布局、输入和呈现位置信息可实时读取。原生建造时间 tooltip 负责游戏侧回调/状态，区别于 Host 的预测显示。 [侧栏](../assist/source/sidebar_indicator_host.h#L12) · [原生 UI](../external_reader/source/local_ui_task.h#L21) · [阵型预览](../assist/source/formation_host.h#L18) · [建造时间](../assist/source/build_time_tooltip.h#L27) |
| 独立单位监控的拥有清单、玩家统计及生产队列 | 独立 Host 工作线程有自己的 `ReadSession`，直接观察运行中的类型、House、拥有数据、工厂和队列。 | 与“战术辅助”里的单位面板是两个入口。它不占游戏回调，但属独立数据采集，见 E04。 [监控入口](../unit_monitor/source/main.cpp#L53) · [工作线程](../unit_monitor/source/sampling_task.h#L31) · [名单和工厂](../unit_monitor/source/sampler.h#L86) |
| 高刷光标、原生光标隐藏、素材与诊断 | 光标模块局部读鼠标对象、窗口及光标素材。素材缓存可读 MIX、松散文件和必要的内存图块。诊断读进程布局、签名或局部状态。 | 这些局部资源、窗口和输入读取无统一场景 token。按素材和现场元数据例外记录。不同于通过独立线程枚举在局单位及工厂。 [光标](../cursor/source/main.cpp#L441) · [原生隐藏](../cursor/source/hide.cpp#L47) · [素材](../external_reader/source/game_asset_files.h#L21) |

## 待迁移例外与现场成本

| 编号 | 条件与线程 | 审计结果和验收点 |
| --- | --- | --- |
| E01 · Ctrl+D 步兵表 | 游戏回调内。开启且有同帧索引时维护乘员历史。按键触发时登记成员，有待准入车辆时重建名单。 | 三处读取均在样本作用域外。步兵表单次上限 16,384。改用同源载员 roster 和可验证的按载具索引后，应回归按一次、同车多乘员、分批下车和缺页延迟。 [历史／准入](../assist/source/native_orders_bridge.h#L111) · [按键](../assist/source/native_orders_bridge.h#L295) |
| E02 · 矿车防蜘蛛的退场证明（pre.11 已迁移） | 游戏回调查询 Worker 封口的同帧 UID 索引。 | 移除原生车辆全表重扫。缺样、跨帧或执行单位身份不匹配时保留任务。明确死亡仍可由目标个体实时证明。 [退场证明](../assist/source/harvester_defense_bridge.h#L333) · [受控回归](../assist/tests/harvester_defense_runtime.cpp) |
| E03 · 类型目录 | 游戏回调内。换局立即触发，目录就绪后每 500 ms 重新取 stamp，未就绪每 100 ms 尝试。只有新 stamp 或首次就绪才调用 `scanTypes`，科技切换也可另触发刷新。 | `catalogue::capture` 的 stamp 会逐项读四类类型表（每表最多 4096），其中包含多个字段和字符串。失败 stamp 保留旧目录。将目录和有效字段投影迁入 Worker 后，应验证局中开关与科技变化刷新。 [调度](../assist/source/bridge.cpp#L1788) · [stamp](../external_reader/source/catalogue_stamp.h#L11) · [目录构造](../assist/source/bridge.cpp#L1031) |
| E04 · 独立单位监控 | Host 工作线程按自身时钟轮询。没有绑定 Runtime 样本，直接读类型、House 拥有量和原生工厂队列。 | 迁移需要同源统计投影，并覆盖已拥有和建造中两类清单。缺样应保持未知并验证暂停、换局与单独打开监控窗口的场景。 [监控轮询](../unit_monitor/source/main.cpp#L478) · [独立读会话](../unit_monitor/source/sampling_task.h#L31) |
| E05 · 原生界面盘点和 Runtime 备用显示 | 游戏回调内。科技侧栏开启时按观测帧检查四条原生图标带，每条最多 75 项，并按目录补齐。持续授予检查本玩家超武列表。正常 Host 会设置 `externalDisplay=1`，关闭该位才走旧显示扫描。 | 科技加强还会按模式独立遍历类型元数据。备用显示用样本锁定选区身份，现场再读状态、乘员容量与投影，实时箱子盘点最多 256 格槽且每 500 ms 更新，生产和超武汇总每 100 ms。记录为模式或回退限定的例外。 [图标盘点](../assist/source/sidebar.h#L162) · [科技元数据](../assist/source/tech_ui_breakpoint_bridge.h#L492) · [备用显示](../assist/source/bridge.cpp#L1467) · [Host 设置](../assist/source/main.cpp#L2624) |
| E06 · 选区和输入读放大 | Host 信息采样每次最多实时拷贝 4096 个选区指针。Runtime 每次走到显示检查的 tick 可实时遍历整个选区算指纹，即使启用了 Host 显示。阵型、巡逻和飞机路径在手势入口还有选区采集。 | 输入事件、命中和最后一致性检查需要当前现场。周期性整份指纹及重复逐对象查找可以从同源选区投影或输入修订号取。需要对大选区统计现场读取次数与时间。 [Host 指纹](../assist/source/main.cpp#L1583) · [Runtime 指纹](../assist/source/bridge.cpp#L1889) · [手势选区](../assist/source/formation_bridge.h#L53) |
| E07 · 调度和首启 | 游戏回调内构造最多 16,384 页的需求并集，首个请求可启动线程。共用严格索引门在多人或高模拟帧率时可能更难赶上当前帧。 | 发现严格索引未到时不会等待 Worker，但 `needsIndex` 分支会提前结束本次派发。部分只读、预览和自动状态推进也会延期。测同帧命中率、请求/发布时延、首启耗时，以及缺样时预览与回执是否继续。 [线程启动](../shared/source/world_frame_command_worker_win.h#L238) · [请求组装](../assist/source/world_frame_runtime.h#L380) · [提前返回](../assist/source/bridge.cpp#L1837) |
| E08 · 同源数据上的游戏回调计算 | 维修候选、装车和逃生配对、躲弹和导弹拦截默认在 Runtime 计算。规划世界投影、矿格分批搜索与原生资格问询也仍有回调内工作。 | 数据来源符合统一采样。大规模事实投影、纯计算及跨候选的原生查验仍会占用游戏线程，实际耗时需单独测。性能验收分开记录现场读取、样本遍历、纯计算与逐对象原生执行。 [计算分配](../assist/source/compute_policy.h#L4) · [游戏回调调用点](../assist/source/bridge.cpp#L1867) |
| E09 · 预定面板的现场轮询 | Host 线程。候选列表同源，服务循环直接读取当前工厂、产品和队列，原生徽标和点击命中直接盘点侧栏。 | 把持续性的界面数据与快照绑定后，仍需要在提交前核验原生工厂与图标。回归新科技出现或消失、切换页签、缺页恢复、队列容量和只提交一次。 [侧栏读取](../assist/source/construction_host.h#L191) · [工厂和队列](../assist/source/construction_host.h#L452) |
| E10 · 手势区域发现 | 游戏输入/回调路径。模糊攻击、区域分配在释放手势或构造指令时实时走过手势附近地图格及格内对象链。 | 属于范围候选发现。应从同源地图与对象投影枚举，将实时读用于输入命中和最终验证。回归多格建筑、同格步兵和拖动时选区变化。 [目标扫描](../assist/source/mo_orders_bridge.h#L123) · [手势提交](../assist/source/mo_orders_bridge.h#L299) |

## 验证

- 已通过 `async_command_no_live_roster.py`、`tactical_frame_sampling.py`、`world_frame_core_test.py`、`construction_host_test.py`。前两项覆盖已迁移的指定路径，不能作为任意功能入口无实时读的证明。核心测试输出的同步图和 Host VisualCache 属于隔离回归夹具。 [测试目录](../assist/tests/)
- 本次是源码审计及合成回归。游戏内 P95/P99、单机高于 60 FPS 时同帧样本命中率、JJ/CnCNet 延迟和断线表现尚未实测。记录中的数量为代码上限或调度阈值，现场触发频率及耗时需要日志验证。
