> 归档补充：下文保留战术首次收尾时的69/3/4分工记录。四项委派的底层源码现已由独立交叉审查补齐，并合并到同目录 tactical-matrix.json；生产预定新增跨局重标发现另见 reservation-world-boundary-review.json。当前汇总结论以总报告为准。

# 战术功能逐项审查报告

## 结论和覆盖口径

当前战术清单 **76个审查单元**已写入 `tactical-matrix.json`：**69项源码路径审查、3项禁用门槛审查、4项业务边界审查并委派底层实现**。本责任范围没有仍标记为 pending 的条目。每项均有实际入口、所查路径、证据文件、发现和验证缺口；并非将模块名称一列计作全部审完。

这里的76是本范围的可对照审查单元，不是全产品功能总数。独立入口或具有不同准入、分配、取消行为的策略分别列项，例如装车两种分配、飞机配额/范围/单目标策略、智能卸载不同乘员接续、六种队形、两种躲弹方式。相同参数控件不额外凑数，范围、上限、类型筛选、按键和状态切换放在对应条目的审查路径内。直接鼠标“攻击移动”和“按键CS”是不同入口，分别列项；它们共用原生移动攻击机制，不因此重复声称底层机制验证。

**确认并修复两处真实生产路径上的二次查找。** 除这两处外，本轮在所审战术业务路径中未确认需要进一步修改的缺陷。源码审查不等于动态执行通过，更不能等于游戏实机或联机通过。

- 审查基线：`c3154fa343a5be42da3453c0b3d228dea653d3ac`。
- 本代理三份生产文件修复由根负责人固化于：`38c6a71f840615986c6d37e1038034183c466566`。
- 生产文件已冻结。最后只更新本目录内矩阵和报告，没有再改 `source/`。
- 本代理未改版本、研究INDEX或远端，也未自行提交。统一x64 Host、x86 Runtime/Broker交叉编译由根负责人完成并单独报告；这里不代报编译结果。

## 清单的实际来源

主要入口取自当前 `tactical_controls.h`、`main.cpp` 的UI/dispatch、`additional_settings.h`、`auto_settings_ui.h`、`automatic_features.h`、`protocol.h` 和 `compute_policy.h`，并追踪到业务接收与最终下单。`tools/build_feature_list.py`仍描述旧1.0.2/70项，引用的`source/FEATURE_CATALOG.json`不存在；未把该脚本或旧模块列表当作完整目录。下文附34个非None Command、32个Flags、13个自动开关和15个计算槽的映射，明确哪些由其他负责人覆盖。

审查维度包括高频工作与上限、ptr/UID/type/owner身份、手动版本和任务仲裁、关闭/模式改变、换局、队列背压、失败重试，以及Host结果的保留请求和时效检查。附加设置、热键解析、公共输入捕获、消息网关和生命周期等基础设施由根负责人覆盖，本报告只保留与战术业务相接的条件。

## TAC-PERF-01：智能卸载每次观察对历史逐个线性查找

**确认路径。** `native_orders_bridge.h::observeUnloadRoster` 在智能卸载开启且通过准入时观察步兵列表。修复前，对每个符合条件的步兵再线性扫描最多8192条卸载历史。满历史、所有步兵已在历史中的排列可产生约33,558,528次指针比较；这是启用后的重复观察路径，不是只在用户发命令时运行。

**为什么没有简单降频。** 历史用于恢复“原生卸载先执行、工具后看见请求”的同帧乘员关系。减少观察频率可能改变捕获和接续语义。因此保留原观察节奏以及历史过期、交换删除、ID/手动版本处理。

**修复。** 在 `unload_policy.h` 增加固定容量 `RosterLookup<8192>`，每次沿用原清理/交换删除后重建ptr→历史下标索引，本轮新增历史时同步登记。历史数组的内容、顺序、容量和淘汰策略保持原行为；只替换成员定位。散列槽数为容量两倍，occupied数量不超过Capacity，清理仅遍历已占槽。碰撞使用有界探测，平均复杂度降为历史重建加步兵扫描；不声称散列表具有严格最坏线性保证。

**边界复核。** 交换删除在重建之前，未使用过期下标；重复ptr更新既有槽不增加occupied；满容量不会越界，零ptr拒绝。新增静态索引约144KiB，无每次堆分配；当前单Runtime工作入口使用，不作为多线程共享容器提供。

**受控验证。** `unload_roster_runtime.py/.cpp`提取基线和修复后的实际`observeUnloadRoster`函数，在相同合成内存/时间条件下运行，逐步比较整个历史数组、顺序和计数。5场景、83项检查通过，涵盖同帧选择与原生先执行、重复列表、过期/交换删除、身份和手动版本、关闭/聊天/所属方/类型、满容量及索引碰撞/清空。8192项、7轮的同一Linux夹具观察值为修复前230.863ms、修复后7.404ms。该数只说明这组受控输入的查找成本，不是实机帧率或Windows速度保证。

证据：`source/assist/source/native_orders_bridge.h`、`source/assist/source/unload_policy.h`、`source/assist/tests/unload_roster_runtime.cpp`、`source/assist/tests/unload_roster_runtime.py`；结果见 `tactical-roster-runtime/result.json`。

## TAC-PERF-02：批量维修Host结果嵌套匹配保留请求

**确认路径。** `bridge.cpp::repairCalculationTick`回收Host维修计算结果时，对每个结果ID扫描一次保留请求全部候选。最多8192×8192；合法同序完整结果约33,558,528次比较，全部不属于请求的结果则最多67,108,864次。运行位置在Runtime回收结果时；自动维修受现有间隔约束，因此这里不是每帧全扫，但大批量结果确实存在二次工作量。

**修复。** 将保留请求ID复制到固定8192项缓冲，排序后对返回ID做二分成员检查。只排序私有索引，不排序请求、结果或后续下单数组；保留Host返回顺序、重复项与原先对零ID的成员判定，后续`repairEvent`仍负责防止重复维修toggle。复杂度变为O(n log n + r log n)，静态缓冲32KiB。

**安全前提已追踪。** `input`来自Runtime私有`repairFrames.find(completed)`，不是直接使用Host可写共享count。`collectRepairs`把candidate.count置0，只在小于8192时递增并由FrameHistory保留；读取值因此在0..8192。沿用结果count检查、序列锁、任务号、match/tick/frame精确匹配、时效与聊天检查。该函数由现有单一已准入Runtime tick经`repairTick`调用，未引入跨线程共享排序使用。

**受控验证。** 扩展既有实际生产函数提取夹具，增加8192项成员检查和空请求。28场景、101项检查通过，涵盖开关关闭后的迟到结果、同帧转手动、关闭再打开代次、背压、重复、计时、换局、满容量返回顺序/未知ID/重复/零值。完整生产源文件及夹具SHA-256在结果中记录，收尾复核与当前冻结修复版本一致。

证据：`source/assist/source/bridge.cpp`、`source/assist/source/compute_frame.h`、`source/assist/source/repair_delivery.h`、`source/assist/tests/auto_repair_runtime.cpp`、`source/assist/tests/auto_repair_runtime.py`；结果见 `tactical-repair-runtime/result.json`。

## 其他路径的具体审查结论和限制

- 智能卸载/自动逃生：检查落地、部署序列、工程师/间谍/普通攻击接续，部署和跟进的有限重试，手动Stop/Enter优先，以及仍在退出运输载具时的保护窗。索引修复未改变这些策略。
- 自动装车/高级进驻：检查当前来源、目标容量、尺寸、原生动作、预留账本和部分成功游标。失败不得先占用成功席位，迟到Host结果不能覆盖手动接管。
- 高级攻击/磁电：检查目标价值或弹头比例、持续任务、原生攻击已生效后的观察、最小射程/停滞、全局仅接管空闲与手动暂停，以及摔落前核实Lift来源/目标关系。原生ABI和hook底层恢复归风险负责人。
- 飞机分配：分别追踪两个数量配额、5格战车覆盖和单目标120%策略；只对成功派出的飞机取消选择，已接受预计伤害跨重规划保留；单目标遇Shift及未能接管原生点击时拒绝。预计伤害不代表实际命中。
- 队形与MO：检查六种布局、点按/轨迹快速布阵、成员不变才延续、精确选择/身份/手动版本、预览节流、合法格、提交预算和寿命。圆形为带中心点的同心环聚集，分散横队为2格间距矩形；没有把它们误记为圆周队列或交错散兵。
- 矿车/蜘蛛防御：检查卸矿前后观察、恢复原采矿或矿场等待、类型清单、FIFO/绕过、手动版本暂停及新对象退休证明。蜘蛛防御始终Runtime；在防御时保留原任务，在退出时有限恢复。矿车列表元数据重建有节流，未把它误算作本次同一高频问题。
- 躲弹/防空：顺向躲弹始终Runtime；原版Scatter才有Host/Runtime两条计算路径。检查有界对象/弹体列表、射击和移动观察、有限窗口、来源重用、手动接管、留枪休眠退出和Guard恢复。防空的256上限多轮分配仍有较大有界最坏工作量，但没有实测证据将它宣称为新性能缺陷。
- 独立命令：选择类只按相应作用域运行；攻击脚下、三种任务模式、喝彩/嘲讽与坚守分别追踪原生命令、会话门槛、失败/冷却。坚守的联机模式仅支持同步进入，未实现单机式再次按键退出；当前UI已说明这一边界。
- 计算模式：当前局使用`computeApplied`；用户修改从下一局生效。Host计算仍由Runtime最终检查和派发；不能把“工具箱计算”理解为绕开游戏身份/手动取消验证。

以上是源码路径结论。两个受控夹具合计33场景/184检查；没有为其他条目额外跑全量历史测试。两份结果均明确`game_process_executed=false`、`windows_runtime_executed=false`。未验证真实图像、声音、弹道、实际战斗伤害、真实地图寻路、Windows线程时序或多人同步。

## 四项委派实现边界

| 矩阵ID | 本代理已核对 | 仍由其他负责人负责 |
|---|---|---|
| damage.native | 低伤害双开关、调用模式、单机/联机作用范围与状态反馈 | 补丁指令、安装所有权、恢复和同步影响 |
| airpath.patch_ui | 原生UI模式分流及兼容命令最终拒绝 | 原生下单/UI补丁安装和恢复 |
| airpath.patch_full | 完整原生模式、arrival开关及兼容分流 | arrival回调/补丁与恢复 |
| queue.reservation | ProduceReserved/Deferred的Runtime目标、建筑/单位队列和工厂准入 | Host服务、取消、预算、失败重试完整状态机 |

三个禁用条目是`retired.autocs`、`retired.c4`、`retired.burst`。确认的是旧配置/邮箱不能恢复活动功能和退休调用未进入当前dispatcher，没有宣称这些功能可用或运行通过。

## 34个Command交叉索引

| Command | 对应条目或责任范围 |
|---|---|
| Repair | repair.manual, repair.auto, repair.unrepairable |
| CombatSelection | select.combat |
| EmptyTransports | select.empty_transport |
| DamagedSelection | select.damaged |
| IdleHarvester | select.idle_miner |
| IdleEngineer | select.idle_engineer |
| IdleTransport | select.idle_transport |
| ClearBeacons | beacons.clear |
| RevealMap | 委派：全图/高风险入口负责人 |
| OpenTech | 委派：科技/高风险入口负责人 |
| SelectSuper | 委派：超武负责人 |
| Produce | 委派：科技/原生生产入口负责人 |
| SelectIron | 委派：超武负责人 |
| SelectChrono | 委派：超武负责人 |
| AutoBoard | load.nearest, load.balanced |
| SelectType | select.type |
| SmartUnload | unload.command, unload.deploy, unload.engineer, unload.spy, unload.attack |
| CancelAdvanced | tasks.cancel |
| SelectVariant | select.variant |
| ProduceReserved | queue.reservation |
| AirPathMove | airpath.compat, airpath.native_events |
| CancelTechProduction | 委派：科技/生产入口负责人 |
| SwitchPlayer | 委派：玩家控制/高风险入口负责人 |
| RestorePlayer | 委派：玩家控制/高风险入口负责人 |
| Cheer | social.cheer |
| Taunt | social.taunt |
| ProduceDeferred | queue.reservation |
| StickyGuard | guard.sticky |
| SetSleep | mission.sleep |
| SetGuard | mission.guard |
| SetHarmless | mission.harmless |
| DestructPlayer | 委派：玩家控制/高风险入口负责人 |
| PlaceCompletedBuilding | 委派：建筑放置/高风险入口负责人 |
| AttackOwnGround | mission.own_ground |

其中21个Command由本代理完整追踪业务路径，2个生产预定Command只审Runtime接收边界，11个由其他负责人负责。

## 32个Flags交叉索引

| Flag | 对应条目或责任范围 |
|---|---|
| Capacity | 委派：容量显示；Host/显示负责人 |
| Iron | 委派：铁幕相关显示/超武；相应负责人 |
| Production | 委派：生产显示；Host/显示负责人 |
| AllTransports | 委派：运输载具显示；Host/显示负责人 |
| Psychic | 委派：心灵相关显示；Host/显示负责人 |
| Crates | 委派：宝箱显示；Host/显示负责人 |
| AutoRepair | repair.auto |
| TextPanel | 委派：文本面板；Host/显示负责人 |
| CrateResults | 委派：宝箱结果显示；Host/显示负责人 |
| AutoCS | retired.autocs |
| NativeOrders | 委派：原生输入观察/接管基础设施；根负责人（战术最终业务条件在各项） |
| EjectAssist | escape.auto |
| UnsafeTech | 委派：科技入口；科技负责人 |
| AdvancedInput | attack.advanced |
| SmartUnloadOrders | unload.command |
| AutoOreReturn | ore.return |
| BuildAnywhere | 委派：任意建筑放置；风险入口负责人 |
| CustomCS | cs.hold |
| MagnetronInput | magnetron.rotate |
| Frozen | 委派：暂停/逻辑门槛；公共Runtime入口负责人 |
| AdvancedEnter | enter.advanced |
| AircraftSpread | air.quota_first, air.quota_second, air.coverage, air.single |
| DiskPrimary | disk.primary |
| SellDrained | sell.drained |
| SellVehicles | sell.vehicle |
| SellEnemy | sell.enemy |
| ExperimentalReveal | 委派：实验全图；全图负责人 |
| RepairUnrepairable | repair.unrepairable |
| ReserveConstruction | queue.reservation |
| AirWaypoints | airpath.compat |
| AntiTheft | defense.antitheft |
| NativeAirWaypoints | airpath.native_events, airpath.patch_ui, airpath.patch_full |

## 13个自动开关交叉索引

| automatic_features Feature | 对应条目 |
|---|---|
| Repair | repair.auto |
| Eject | escape.auto |
| MagnetronDrop | magnetron.drop |
| ConstructionQueue | queue.reservation |
| OreReturn | ore.return |
| MinerDefense | miner.spider |
| AntiTheft | defense.antitheft |
| Dodge | dodge.forward, dodge.scatter |
| MissileDefense | missile.targets, missile.smart, missile.balanced |
| MissileReserve | missile.reserve |
| LowDamageFilter | damage.filter, damage.native |
| MagnetronGlobal | magnetron.global |
| AutoCS | retired.autocs |

## 15个计算槽交叉索引

BurstRefresh是隐藏退休兼容槽，其余14个在当前计算模式UI中可选。

| compute Feature | 对应条目 | 实际路径 |
|---|---|---|
| Advanced | attack.advanced | Host / Runtime，共用规划与最终准入 |
| Magnetron | magnetron.rotate | Host / Runtime；与全局磁电的计算选择独立 |
| Ore | ore.return | Host / Runtime规划；卸矿后观察/恢复采矿仍由Runtime执行 |
| Defense | defense.antitheft | Host / Runtime，共用规划与最终准入 |
| Enter | enter.advanced | Host / Runtime，共用分配和最终容量检查 |
| Aircraft | air.quota_first, air.quota_second, air.coverage, air.single | Host / Runtime，共用分配和最终准入 |
| Repair | repair.manual, repair.auto, repair.unrepairable | Host / Runtime；本次修复为Host结果回收路径 |
| Load | load.nearest, load.balanced | Host / Runtime，Runtime保留任务与容量账本 |
| Unload | unload.command, unload.deploy, unload.engineer, unload.spy, unload.attack | Host / Runtime，Runtime观察落地及派发 |
| Escape | escape.auto | Host / Runtime，Runtime观察和接续 |
| MissileDefense | missile.targets, missile.smart, missile.balanced, missile.reserve | Host / Runtime，共用纯计算和Runtime最终检查 |
| BurstRefresh | retired.burst | 退休兼容槽，UI隐藏且无活动调用 |
| Dodge | dodge.forward, dodge.scatter | Scatter可选Host / Runtime；顺向移动始终Runtime |
| MagnetronGlobal | magnetron.global | Host / Runtime；只接管空闲来源 |
| MagnetronDrop | magnetron.drop | Host / Runtime；Runtime核实当前Lift关系 |

## 76个审查单元索引

逐项详细入口、检查路径、证据与缺口以同目录JSON为准。

| ID | 实际功能/策略 | 状态 |
|---|---|---|
| repair.manual | 手动批量维修 | 源码路径已审 |
| repair.auto | 自动维修 | 源码路径已审 |
| repair.unrepairable | 允许维修通常不可维修建筑 | 源码路径已审 |
| select.combat | 选择作战单位 | 源码路径已审 |
| select.empty_transport | 选择空载运输载具 | 源码路径已审 |
| select.damaged | 选择受损单位 | 源码路径已审 |
| select.idle_miner | 轮选空闲矿车 | 源码路径已审 |
| select.idle_engineer | 轮选空闲工程师 | 源码路径已审 |
| select.idle_transport | 定位空闲运输载具 | 源码路径已审 |
| select.idle_aircraft | 轮选空闲飞机 | 源码路径已审 |
| select.type | 组合选兵：同类型单按屏幕／双按全图 | 源码路径已审 |
| select.variant | 按当前武器选择同类单位 | 源码路径已审 |
| beacons.clear | 清除信标 | 源码路径已审 |
| load.nearest | 自动装车：就近分配 | 源码路径已审 |
| load.balanced | 自动装车：兵种均衡 | 源码路径已审 |
| unload.command | 智能卸载：所选载具卸载与原生先执行竞态 | 源码路径已审 |
| unload.deploy | 智能卸载：部署兵落地部署与重试 | 源码路径已审 |
| unload.engineer | 智能卸载：工程师后续任务 | 源码路径已审 |
| unload.spy | 智能卸载：间谍后续任务 | 源码路径已审 |
| unload.attack | 智能卸载：普通乘员后续攻击 | 源码路径已审 |
| escape.auto | 载员自动逃生与落地接续 | 源码路径已审 |
| attack.advanced | 高级移动攻击与持续索敌 | 源码路径已审 |
| magnetron.rotate | 磁电轮换攻击 | 源码路径已审 |
| magnetron.global | 磁电全局无限吸 | 源码路径已审 |
| magnetron.drop | 磁电全局自动摔落 | 源码路径已审 |
| enter.advanced | 高级进驻分配 | 源码路径已审 |
| air.quota_first | 飞机分配：数量配额模式1 | 源码路径已审 |
| air.quota_second | 飞机分配：数量配额模式2 | 源码路径已审 |
| air.coverage | 飞机分配：5格战车预计伤害120% | 源码路径已审 |
| air.single | 飞机分配：单目标120%与成功派出后取消选中 | 源码路径已审 |
| disk.primary | 飞碟主武器攻击 | 源码路径已审 |
| sell.drained | 出售被吸取目标 | 源码路径已审 |
| sell.vehicle | 出售战车 | 源码路径已审 |
| sell.enemy | 出售敌方建筑（旧配置/风险入口） | 源码路径已审 |
| attack.direct | 直接攻击移动 | 源码路径已审 |
| cs.hold | 手动按住CS移动攻击 | 源码路径已审 |
| tasks.cancel | 停止所选单位自动任务 | 源码路径已审 |
| mo.fuzzy | 模糊下令分组攻击 | 源码路径已审 |
| mo.region | 区域目标分配 | 源码路径已审 |
| mo.patrol | 自定义巡逻圈与原生计划提交 | 源码路径已审 |
| formation.line | 队形：矩形横队 | 源码路径已审 |
| formation.spread | 队形：分散横队 | 源码路径已审 |
| formation.circle | 队形：圆形聚集 | 源码路径已审 |
| formation.fan | 队形：60度扇形 | 源码路径已审 |
| formation.square | 队形：正方形聚集 | 源码路径已审 |
| formation.hollow | 队形：空心框聚集 | 源码路径已审 |
| formation.continue | 成员不变时后续移动保持队形 | 源码路径已审 |
| formation.quick_point | 快速布阵点按聚集 | 源码路径已审 |
| formation.quick_curve | 快速布阵轨迹排布（直线／曲线） | 源码路径已审 |
| guard.sticky | 坚守位置/黏住警戒 | 源码路径已审 |
| mission.sleep | 不攻击模式 | 源码路径已审 |
| mission.guard | 驻守模式 | 源码路径已审 |
| mission.harmless | 无害模式 | 源码路径已审 |
| mission.own_ground | 攻击脚下 | 源码路径已审 |
| ore.return | 矿车自动卸矿与恢复采矿 | 源码路径已审 |
| ore.custom_types | 矿车/矿场独立类型清单 | 源码路径已审 |
| miner.spider | 自动点杀蜘蛛与恢复原任务 | 源码路径已审 |
| defense.antitheft | 防御建筑优先攻击渗透单位 | 源码路径已审 |
| dodge.forward | 自动躲弹：顺向移动两格 | 源码路径已审 |
| dodge.scatter | 自动躲弹：原版Scatter | 源码路径已审 |
| missile.targets | 防空火控目标：仅导弹类/全部敌方空中单位 | 源码路径已审 |
| missile.smart | 防空火控智能分配 | 源码路径已审 |
| missile.balanced | 防空火控平均分配 | 源码路径已审 |
| missile.reserve | 提前休眠留枪与退出恢复Guard | 源码路径已审 |
| damage.filter | 过滤工具箱低伤害自动目标 | 源码路径已审 |
| damage.native | 高危修改原生低伤害索敌 | 只审边界，底层委派 |
| airpath.compat | 飞机兼容路径点接续 | 源码路径已审 |
| airpath.native_events | 飞机原生事件兼容 | 源码路径已审 |
| airpath.patch_ui | 飞机原生下单补丁 | 只审边界，底层委派 |
| airpath.patch_full | 飞机完整原生补丁 | 只审边界，底层委派 |
| queue.reservation | 生产预定：建筑/防御与单位队列 | 只审边界，底层委派 |
| social.cheer | 喝彩 | 源码路径已审 |
| social.taunt | 嘲讽1–8 | 源码路径已审 |
| retired.autocs | 自动移动攻击CS（暂不可用） | 只审禁用门槛 |
| retired.c4 | 步兵强制进入建筑（暂不可用） | 只审禁用门槛 |
| retired.burst | 强制刷弹（退休兼容槽） | 只审禁用门槛 |

## 交付文件

- `tactical-matrix.json`：76项完整逐点证据与验证缺口。
- `tactical-status.json`：统计、冻结版本与无pending状态，供根负责人汇总。
- `tactical-repair-runtime/result.json`：28场景/101项检查及生产/夹具源哈希。
- `tactical-roster-runtime/result.json`：5场景/83项检查、旧新逐字节比较和合成基准。
