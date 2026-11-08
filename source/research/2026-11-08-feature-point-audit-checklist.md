# RA2 Toolbox 逐功能点审查清单

基线 `c3154fa343a5be42da3453c0b3d228dea653d3ac`；修复源码 `d9d7056c6e0faeef60507f8c2c7f7c847dc8af26`（与本地完整构建 `491bbc72ec67ac70454e9d9a0ef69287eb1ba28f` 的 Git 根树一致）。

每行是一次审查单元，可包含多个模式或支撑机制；各组有交叉，不能把行数相加称作独立产品功能总量，也不能据此计算实机通过率。

“源码路径已审”表示已核对所列入口、实现与生命周期；没有运行记录的项目没有被视为运行通过。禁用门槛与保留配置单列。所有 Windows、真实游戏、GPU、联机和任意组合验证仍须单独验收。

完整路径、证据、发现和验证缺口保存在各组 JSON；结论与剩余问题见 [总报告](2026-11-08-feature-point-audit.md)。

## 战术、自动化与快捷键（76 条审查记录）

[完整机器可读明细](evidence/feature-point-audit-20261108/tactical-matrix.json)

| ID | 功能或机制 | 模式 / 范围 | 审查状态 |
| --- | --- | --- | --- |
| repair.manual | 手动批量维修 | Host计算；Runtime计算；自动周期500–30000毫秒；RepairBuilding类型清单 | 源码路径已审 |
| repair.auto | 自动维修 | Host计算；Runtime计算；自动周期500–30000毫秒；RepairBuilding类型清单 | 源码路径已审 |
| repair.unrepairable | 允许维修通常不可维修建筑 | Host计算；Runtime计算；自动周期500–30000毫秒；RepairBuilding类型清单 | 源码路径已审 |
| select.combat | 选择作战单位 | 筛选当前选中单位 | 源码路径已审 |
| select.empty_transport | 选择空载运输载具 | 从全局对象名单选择自有空载运输载具 | 源码路径已审 |
| select.damaged | 选择受损单位 | 筛选当前选中单位 | 源码路径已审 |
| select.idle_miner | 轮选空闲矿车 | 一次性快捷操作 | 源码路径已审 |
| select.idle_engineer | 轮选空闲工程师 | 一次性快捷操作 | 源码路径已审 |
| select.idle_transport | 定位空闲运输载具 | 一次性快捷操作 | 源码路径已审 |
| select.idle_aircraft | 轮选空闲飞机 | 一次性快捷操作 | 源码路径已审 |
| select.type | 组合选兵：同类型单按屏幕／双按全图 | 单按屏幕；双按全图；组合类型掩码；双按间隔20–5000毫秒 | 源码路径已审 |
| select.variant | 按当前武器选择同类单位 | 屏幕范围；全图范围；当前武器变体 | 源码路径已审 |
| beacons.clear | 清除信标 | 显式一次性操作 | 源码路径已审 |
| load.nearest | 自动装车：就近分配 | Host计算；Runtime计算；范围1–512格；乘员/载具独立类型清单 | 源码路径已审 |
| load.balanced | 自动装车：兵种均衡 | Host计算；Runtime计算；范围1–512格；乘员/载具独立类型清单 | 源码路径已审 |
| unload.command | 智能卸载：所选载具卸载与原生先执行竞态 | Host计算；Runtime计算；独立乘员/载具类型清单；显式卸载 | 源码路径已审 |
| unload.deploy | 智能卸载：部署兵落地部署与重试 | Host计算；Runtime计算；独立乘员/载具类型清单；显式卸载 | 源码路径已审 |
| unload.engineer | 智能卸载：工程师后续任务 | Host计算；Runtime计算；独立乘员/载具类型清单；显式卸载 | 源码路径已审 |
| unload.spy | 智能卸载：间谍后续任务 | Host计算；Runtime计算；独立乘员/载具类型清单；显式卸载 | 源码路径已审 |
| unload.attack | 智能卸载：普通乘员后续攻击 | Host计算；Runtime计算；独立乘员/载具类型清单；显式卸载 | 源码路径已审 |
| escape.auto | 载员自动逃生与落地接续 | Host计算；Runtime计算；独立乘员/载具类型清单；自动载员逃生 | 源码路径已审 |
| attack.advanced | 高级移动攻击与持续索敌 | Host计算；Runtime计算；价值/弹头效果优先；检测1–60Hz；额外索敌0–30格 | 源码路径已审 |
| magnetron.rotate | 磁电轮换攻击 | Host计算；Runtime计算；独立类型清单 | 源码路径已审 |
| magnetron.global | 磁电全局无限吸 | Host计算；Runtime计算；独立类型清单 | 源码路径已审 |
| magnetron.drop | 磁电全局自动摔落 | Host计算；Runtime计算；独立类型清单 | 源码路径已审 |
| enter.advanced | 高级进驻分配 | Host计算；Runtime计算；cooperative::Enter | 源码路径已审 |
| air.quota_first | 飞机分配：数量配额模式1 | Host计算；Runtime计算；cooperative::AirFirst；配额0–512；0最大值表示不限；目标类型配额覆盖 | 源码路径已审 |
| air.quota_second | 飞机分配：数量配额模式2 | Host计算；Runtime计算；cooperative::AirSecond；配额0–512；0最大值表示不限；目标类型配额覆盖 | 源码路径已审 |
| air.coverage | 飞机分配：5格战车预计伤害120% | Host计算；Runtime计算；cooperative::AirDamage | 源码路径已审 |
| air.single | 飞机分配：单目标120%与成功派出后取消选中 | Host计算；Runtime计算；cooperative::AirSingle | 源码路径已审 |
| disk.primary | 飞碟主武器攻击 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| sell.drained | 出售被吸取目标 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| sell.vehicle | 出售战车 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| sell.enemy | 出售敌方建筑（旧配置/风险入口） | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| attack.direct | 直接攻击移动 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| cs.hold | 手动按住CS移动攻击 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| tasks.cancel | 停止所选单位自动任务 | 显式一次性增强操作；原生事件/任务取消 | 源码路径已审 |
| mo.fuzzy | 模糊下令分组攻击 | Fuzzy；按键/鼠标独立绑定；预览与释放提交 | 源码路径已审 |
| mo.region | 区域目标分配 | Area；按键/鼠标独立绑定；预览与释放提交 | 源码路径已审 |
| mo.patrol | 自定义巡逻圈与原生计划提交 | Patrol；按键/鼠标独立绑定；预览与释放提交 | 源码路径已审 |
| formation.line | 队形：矩形横队 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.spread | 队形：分散横队 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.circle | 队形：圆形聚集 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.fan | 队形：60度扇形 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.square | 队形：正方形聚集 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.hollow | 队形：空心框聚集 | Runtime执行；原生同步Move；独立形状键/按住与双按激活 | 源码路径已审 |
| formation.continue | 成员不变时后续移动保持队形 | Runtime执行；原生同步Move；继续保持选中队形 | 源码路径已审 |
| formation.quick_point | 快速布阵点按聚集 | Runtime执行；原生同步Move；快速布阵独立手势 | 源码路径已审 |
| formation.quick_curve | 快速布阵轨迹排布（直线／曲线） | Runtime执行；原生同步Move；快速布阵独立手势 | 源码路径已审 |
| guard.sticky | 坚守位置/黏住警戒 | 单机本地原生状态入口；联机同步mission6观察 | 源码路径已审 |
| mission.sleep | 不攻击模式 | 单机/联机原生同步事件 | 源码路径已审 |
| mission.guard | 驻守模式 | 单机/联机原生同步事件 | 源码路径已审 |
| mission.harmless | 无害模式 | 单机/联机原生同步事件 | 源码路径已审 |
| mission.own_ground | 攻击脚下 | 原生对象/格攻击；单机/联机事件 | 源码路径已审 |
| ore.return | 矿车自动卸矿与恢复采矿 | 返矿分配Host/Runtime；卸矿后恢复采矿Runtime观察；矿场/矿车默认识别或类型清单 | 源码路径已审 |
| ore.custom_types | 矿车/矿场独立类型清单 | 默认识别；矿车清单；矿场清单；Host/Runtime共享配置 | 源码路径已审 |
| miner.spider | 自动点杀蜘蛛与恢复原任务 | Runtime游戏线程；默认矿车；自定义地面单位；额外索敌0–30格；退出多1格 | 源码路径已审 |
| defense.antitheft | 防御建筑优先攻击渗透单位 | Host计算；Runtime计算；默认/自定义防御类型 | 源码路径已审 |
| dodge.forward | 自动躲弹：顺向移动两格 | Runtime执行（不经过Host算法往返）；顺向两格；逼近碾压仍使用同步Scatter | 源码路径已审 |
| dodge.scatter | 自动躲弹：原版Scatter | Host计算；Runtime计算；原生同步Scatter | 源码路径已审 |
| missile.targets | 防空火控目标：仅导弹类/全部敌方空中单位 | Host计算；Runtime计算；目标范围两种；智能/平均分配；提前Sleep开关 | 源码路径已审 |
| missile.smart | 防空火控智能分配 | Host计算；Runtime计算；目标范围两种；智能/平均分配；提前Sleep开关 | 源码路径已审 |
| missile.balanced | 防空火控平均分配 | Host计算；Runtime计算；目标范围两种；智能/平均分配；提前Sleep开关 | 源码路径已审 |
| missile.reserve | 提前休眠留枪与退出恢复Guard | Host计算；Runtime计算；目标范围两种；智能/平均分配；提前Sleep开关 | 源码路径已审 |
| damage.filter | 过滤工具箱低伤害自动目标 | 工具箱候选过滤；Host/Runtime共用候选 | 源码路径已审 |
| damage.native | 高危修改原生低伤害索敌 | 单机；联机所有阵营；默认关闭；两开关同时开启才安装（SP也要求原生开关）；SP仅当前玩家所属单位；MP所有阵营；仅工具过滤不安装原生补丁；关闭/暂停/换局/控制器退出/故障 | 源码路径已审 |
| airpath.compat | 飞机兼容路径点接续 | 单机/联机普通同步事件；兼容路线 | 源码路径已审 |
| airpath.native_events | 飞机原生事件兼容 | 单机/联机普通同步事件；原生键触发的事件兼容 | 源码路径已审 |
| airpath.patch_ui | 飞机原生下单补丁 | 单机实验；联机实验；UI路径点模式3 / nativeAirNetwork=1；SP会话0/5；MP会话3/4（实验）；自定义路径键转兼容路径；关闭/失焦或暂停/换局/控制器退出/故障 | 源码路径已审 |
| airpath.patch_full | 飞机完整原生补丁 | 单机实验；联机实验；UI路径点模式4 / nativeAirNetwork=2；SP会话0/5；MP会话3/4（实验）；自定义路径键转兼容路径；关闭/失焦或暂停/换局/控制器退出/故障 | 源码路径已审 |
| queue.reservation | 生产预定：建筑/防御与单位队列 | ProduceReserved；ProduceDeferred；建筑/防御双通道；单位多工厂通道；建筑/防御两个FIFO，32项；步兵/载具两个展示FIFO，128项；步兵/陆军/海军/飞机四个工厂预算通道；可见堆叠5项；普通/未来前置/科技白名单候选；面板/原生侧栏点选、右键取消、Shift+右键清空、原生Tab快捷键；ProduceReserved/ProduceDeferred；暂停/失焦/关显示/关功能/换局/换进程/连接失败 | 源码已审；有未闭合发现 |
| social.cheer | 喝彩 | 原生喝彩 | 源码路径已审 |
| social.taunt | 嘲讽1–8 | 联机嘲讽1–8 | 源码路径已审 |
| retired.autocs | 自动移动攻击CS（暂不可用） | 禁用兼容入口 | 禁用门槛已审 |
| retired.c4 | 步兵强制进入建筑（暂不可用） | 禁用兼容入口 | 禁用门槛已审 |
| retired.burst | 强制刷弹（退休兼容槽） | 退休兼容槽 | 禁用门槛已审 |

## 显示、光标、监视与渲染（152 条审查记录）

[完整机器可读明细](evidence/feature-point-audit-20261108/render-matrix.json)

| ID | 功能或机制 | 模式 / 范围 | 审查状态 |
| --- | --- | --- | --- |
| display.RefreshMode | RefreshMode | 只读刷新（0）；旧场景触发入口已强制返回0 | 源码路径已审 |
| display.EnemyMarkers | EnemyMarkers | 关闭；开启 | 已修复；受控验证通过 |
| display.Iron | Iron | 关闭；开启 | 源码路径已审 |
| display.Capacity | Capacity | 关闭；开启 | 源码路径已审 |
| display.AllTransports | AllTransports | 关闭；开启 | 源码路径已审 |
| display.Frozen | Frozen | 关闭；开启 | 源码路径已审 |
| display.Parasite | Parasite | 关闭；开启 | 已修复；受控验证通过 |
| display.CrateBuffs | CrateBuffs | 关闭；开启 | 已修复；受控验证通过 |
| display.LabelScale | LabelScale | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.InfoFPS | InfoFPS | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.ShroudStrategy | ShroudStrategy | 0；1；2；3；4（具体显示策略待追踪） | 源码路径已审 |
| display.PassengerBubbles | PassengerBubbles | 关闭；开启 | 源码路径已审 |
| display.DangerPassengers | DangerPassengers | 关闭；开启 | 源码路径已审 |
| display.InfoAdaptive | InfoAdaptive | 关闭；开启 | 源码路径已审 |
| display.RenderUpgrade | 渲染升级（入口暂不可用） | 入口禁用，保留已保存启用状态；关闭/暂停/游戏菜单/失焦/换局；已保存启用时主世界重投影与完整层原子发布 | 源码路径已审 |
| display.RenderProcessing | RenderProcessing | 关闭；开启 | 源码路径已审 |
| display.RenderPhysical | 按显示器物理像素绘制 | 关闭/开启偏好；仅在 RenderUpgrade 已开时允许 UI 修改；物理几何符合/窗口化/显示模式切换/DPI不一致/失效 | 源码路径已审 |
| display.RenderUnified | 合并世界信息与监控覆盖层 | 关闭；开启 | 源码路径已审 |
| display.RenderPalette | RenderPalette | 0；1（配置保留，当前 effective 参数固定为0） | 源码路径已审 |
| display.RenderPrecision | RenderPrecision | 0；1（配置保留，当前 effective 参数固定为0） | 源码路径已审 |
| display.RenderMaterial | RenderMaterial | 关闭；开启 | 源码路径已审 |
| display.RenderDetail | RenderDetail | 0；1（配置保留，当前 effective 参数固定为0） | 源码路径已审 |
| display.RenderXbrz | RenderXbrz | 关闭；开启 | 源码路径已审 |
| display.RenderScale | RenderScale | 0；1；2；3（配置保留，当前 effectiveRenderScale 固定为1） | 源码路径已审 |
| display.RenderGPU | Vulkan GPU（主视图、副相机、小地图） | CPU；Vulkan计算后CPU像素读回发布；GPU不可用/预算拒绝/超时/故障时整帧CPU回放；共享图像直出当前编译期关闭 | 源码路径已审 |
| display.RenderParticles | 粒子与动画效果 | 关闭/开启一般动画、粒子、VXL动画、glow/spotlight/instant light/alpha与采矿效果；基础弹体与建筑附属动画有独立保留路径；GPU计划或CPU同序绘制 | 源码路径已审 |
| display.RenderLasers | 激光与光束效果 | 关闭/开启激光、辐射束、波纹和电弧；RGB555/565及原生detail分支；GPU效果计划/CPU绘制 | 源码路径已审 |
| display.RenderVoxelLighting | VXL 实时光影重算 | 实时VPL重算开启；关闭后保留已验证光照表；调色板/VPL不可读时拒绝必要世界产物 | 源码路径已审 |
| display.RenderSpriteLighting | SHP 实时光影重算 | SHP实时对象/地形光照开启；关闭实时对象重算；调色板变化/不可读与瞬态资源过期 | 源码路径已审 |
| display.WatchPrimary | 在游戏内显示副相机 | 关闭；开启 | 源码路径已审 |
| display.WatchSecondary | 在第二显示器显示副相机 | 关闭；开启 | 源码路径已审 |
| display.WatchX | 副屏相机组水平位置 / 万分比 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.WatchY | 副屏相机组垂直位置 / 万分比 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.WatchDanger | 自动跟踪危险载员 | 关闭；开启 | 源码路径已审 |
| display.WatchKey1 | 副相机 1 快捷键 | 未绑定；单键；修饰键组合 | 源码路径已审 |
| display.WatchKey2 | 副相机 2 快捷键 | 未绑定；单键；修饰键组合 | 源码路径已审 |
| display.WatchKey3 | 副相机 3 快捷键 | 未绑定；单键；修饰键组合 | 源码路径已审 |
| display.MapPrimary | 在游戏内显示小地图 | 关闭；开启 | 源码路径已审 |
| display.MapSecondary | 在第二显示器显示小地图 | 关闭；开启 | 源码路径已审 |
| display.MapMode | 小地图内容 | 示意；实景 | 源码路径已审 |
| display.MapScale | 游戏内小地图缩放 / % | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.MapPosition | 游戏内小地图位置 | 原雷达左侧；左上；左下；自定义 | 源码路径已审 |
| display.MapX | 自定义 X / 像素 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.MapY | 自定义 Y / 像素 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.MapAlignX | 水平对齐 | 靠左/上；居中；靠右/下 | 源码路径已审 |
| display.MapAlignY | 垂直对齐 | 靠左/上；居中；靠右/下 | 源码路径已审 |
| display.MapSecondaryX | 副屏小地图水平位置 / 万分比 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.MapSecondaryY | 副屏小地图垂直位置 / 万分比 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.MapSecondaryWidth | 副屏小地图宽度 / 像素 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.ProductionHints | 生产队列空闲提醒 | 关闭；开启 | 源码路径已审 |
| display.PsychicHints | 敌方索敌信息 | 关闭；开启 | 源码路径已审 |
| display.CrateHints | 箱子信息 | 关闭；开启 | 源码路径已审 |
| display.TextHints | 战术文字提示 | 关闭；开启 | 源码路径已审 |
| display.IdleAircraft | 空闲飞机指示器 | 关闭；开启 | 源码已审；有未闭合发现 |
| display.ReminderBuilding | 提醒建筑队列 | 关闭；开启 | 源码路径已审 |
| display.ReminderDefense | 提醒防御队列 | 关闭；开启 | 源码路径已审 |
| display.ReminderInfantry | 提醒步兵队列 | 关闭；开启 | 源码路径已审 |
| display.ReminderVehicle | 提醒载具队列 | 关闭；开启 | 源码路径已审 |
| display.ReminderShip | 提醒舰船队列 | 关闭；开启 | 源码路径已审 |
| display.ReminderAircraft | 提醒飞机队列 | 关闭；开启 | 源码路径已审 |
| display.TextMargin | 文字提示上边距 / 像素 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.ToastDuration | 战术提示时长 / 毫秒 | 下界；中间值；上界；非法/未完成输入 | 源码路径已审 |
| display.RenderHighRefresh | 高刷移动补帧（只读帧号取样） | 关闭：最新观测位置；开启：最新两帧驱动的有界显示位置前推；同帧重复/长暂停/换世界/不连续对象/瞬移回退 | 源码路径已审 |
| display.HarvesterCounter | 矿车指示器（工作中 / 总数） | 关闭；开启 | 源码路径已审 |
| display.PowerDelta | 电量指示器（发电 − 耗电） | 关闭；开启 | 源码路径已审 |
| display.BuildTimeTooltip | 原生建造栏提示框显示建造时间 | 关闭/开启；建筑/防御栏目；总时长/剩余时间/就绪/暂停/资金不足；单机/联机名义速率与自定义单机FPS；与Cursor共享文字槽并独立释放 | 源码已审；有未闭合发现 |
| display.OverlayToggleKey | 所有覆盖层显隐键（默认 · / `） | 未绑定；单键；修饰键组合 | 源码路径已审 |
| display.SinglePlayerFPS | 单机游戏 FPS（0 = 原版，10–240） | 0原版；10–240；非法值；联机拒绝 | 源码路径已审 |
| cursor.connect | 连接游戏 / 手工重连 | 自动发现；显式PID；进程退出/同PID重用 | 源码路径已审 |
| cursor.enable | 启用、暂停、停止光标 | 主按钮；开始/停止；自动暂停 | 源码路径已审 |
| cursor.autostart | 自动连接 | 开；关；游戏退出再启动 | 源码路径已审 |
| cursor.visibility_mode | 原光标处理 | 保留原光标；隐藏原光标；透明SHA；无光标诊断 | 源码路径已审 |
| cursor.native_delivery | 光标显示方式 | 外置层；游戏内原生HCURSOR；映射时外置层回退 | 源码路径已审 |
| cursor.position | 光标位置 | 自动匹配游戏；系统鼠标；游戏画布拉伸到全屏 | 源码路径已审 |
| cursor.scale | 图像缩放 | 25–400%；预置比例；非法输入 | 源码路径已审 |
| cursor.hotspot_x | 热点 X | -256–256；原图/指定边缘 | 源码路径已审 |
| cursor.hotspot_y | 热点 Y | -256–256；原图/指定边缘 | 源码路径已审 |
| cursor.auto_scale | 自动缩放 | 开；关 | 源码路径已审 |
| cursor.aspect | 保持纵横比 | 开；关 | 源码路径已审 |
| cursor.game_canvas | 游戏尺寸覆盖 | 自动0/0；有效宽高；单边无效 | 源码路径已审 |
| cursor.screen_canvas | 系统尺寸覆盖 | 自动0/0；有效宽高；单边无效 | 源码路径已审 |
| cursor.dpi_override | Windows缩放覆盖 | 自动0；50–500% | 源码路径已审 |
| cursor.dpi_mode | 缩放计算 | 物理像素；系统缩放；抵消缩放 | 源码路径已审 |
| cursor.smooth | 动画平滑补帧 | 开；关 | 源码路径已审 |
| cursor.adaptive | 动画自适应时序 | 开；关 | 源码路径已审 |
| cursor.blend_ms | 备用补帧周期 | 10–200ms | 源码路径已审 |
| cursor.poll_rate | 光标目标刷新率 | 自动；30–500Hz；非法输入 | 源码路径已审 |
| cursor.super_center | 超级武器视觉中心 | 开；关 | 源码路径已审 |
| cursor.style | 光标样式选择 | 原版；内置；自定义 | 源码路径已审 |
| cursor.import | 导入SHA与色盘 | palette.bgra；mousepal.pal；手工选色盘 | 源码路径已审 |
| cursor.prepare | 补帧预处理 | 开始；取消；成功；失败；处理时切换样式 | 源码路径已审 |
| cursor.groups | 打开动画分组文件 | 查看/编辑 groups.json | 源码路径已审 |
| cursor.preview | 动画预览 | 打开当前profile的preview.html；预览文件缺失时提示 | 源码路径已审 |
| cursor.hud | 诊断HUD | 开；关 | 源码路径已审 |
| cursor.advanced | 高级设置展开收起 | 紧凑；高级；DPI/滚动 | 源码路径已审 |
| cursor.install_sha | 透明SHA安装 | 首次备份；已有自定义SHA；正在运行的游戏 | 源码路径已审 |
| cursor.restore_sha | 恢复原SHA | 工具拥有备份；外部修改冲突 | 源码路径已审 |
| cursor.game_dir | 选择游戏目录 | 目录选择；游戏中更换 | 源码路径已审 |
| cursor.shift_five | Shift五连点兼容 | 自动判Ares；强制；关闭；鼠标捕获/取消 | 源码路径已审 |
| cursor.shift_gap | Shift五连点间隔 | 0–250ms | 源码路径已审 |
| cursor.shift_region | Shift五连点侧栏限制 | 自动识别；SidebarWidth/Top保留配置 | 源码路径已审 |
| cursor.ddraw_apply | DDraw高刷配置 | CnC-DDraw；TS-DDraw；未知或多个模块拒绝 | 源码路径已审 |
| cursor.ddraw_restore | DDraw恢复 | 原文件未外改；已外改拒绝 | 源码路径已审 |
| cursor.native_cache | 原生光标资源与HCURSOR缓存 | 尺寸/热点/帧/颜色；原资源重新载入 | 源码路径已审 |
| cursor.snapshot_latch | 光标快照锁存 | 新样本；旧样本；切换进程/实例 | 源码路径已审 |
| cursor.visibility_restore | 失焦/菜单/关闭光标恢复 | 失焦；非战场；退出；隐藏模式交接 | 源码路径已审 |
| cursor.playback_cache | 预处理动画缓存 | 磁盘命中；缺失/损坏；运行生成；内存限额 | 源码路径已审 |
| monitor.enabled | 主屏统计启用/暂停 | 开；关 | 源码路径已审 |
| monitor.demo | 布局演示 | 进入；退出；模拟玩家退场 | 源码路径已审 |
| monitor.reset | 重新连接统计 | 活跃采样；切换进程 | 源码路径已审 |
| monitor.buildings | 显示已有建筑 | 开；关；生产中建筑保留 | 源码路径已审 |
| monitor.others | 显示其他玩家 | 开；关 | 源码路径已审 |
| monitor.credits | 各玩家资金 | 已知金额；读取失败显示未知 | 源码路径已审 |
| monitor.owned | 单位拥有数量 | 步兵；载具；飞机；建筑；乘客/驻军 | 源码路径已审 |
| monitor.queues | 生产队列数量 | 排队；正在生产；工厂共有队列 | 源码路径已审 |
| monitor.progress | 生产进度与暂停 | 0–54；暂停/继续 | 源码路径已审 |
| monitor.players | 玩家筛选/退场压缩 | 本地优先；人类/AI；观战；败退；换局 | 源码路径已审 |
| monitor.top | 统计上边距 | 0–4096 | 源码路径已审 |
| monitor.left | 统计左边距 | 0–4096 | 源码路径已审 |
| monitor.scale | 统计整体缩放 | 50–250%；非法/未完成输入 | 源码路径已审 |
| monitor.paging | 统计分页 | 上一页；下一页；各玩家独立页数 | 源码路径已审 |
| monitor.show_hide | 显隐/常驻 | 统一覆盖层键；单独版Tab/按钮；显示/隐藏 | 源码路径已审 |
| monitor.tab_mode | Tab行为 | 独立版：切换；独立版：按住；独立版：短按切换长按临时；统一版：禁用Tab控件并消费共享覆盖层键 | 源码路径已审 |
| monitor.secondary | 副屏统计 | 开；关；副屏消失；游戏后台 | 源码路径已审 |
| monitor.secondary_x | 副屏横向位置 | 0–10000；拖动 | 源码路径已审 |
| monitor.secondary_y | 副屏纵向位置 | 0–10000；拖动 | 源码路径已审 |
| monitor.icons | SHP图标缓存与调色板 | 原生图；磁盘缓存；占位符；资源变化 | 源码路径已审 |
| monitor.async | 统计异步采样/绘制提交 | 样本替换；相同可见内容跳过；过期丢弃；关闭 | 源码路径已审 |
| backend.main_world | 主画面保留世界重投影 | 共享不可变世界；视口当前重投影；同source token复用；相机/尺寸/世界/profile变化失效 | 源码路径已审 |
| backend.terrain_cache | 地形瓦片与页面缓存 | 全图保留地形；当前视口有界覆盖；TMP/颜色页缓存；palette/theater/resource/geometry变化与失败回退 | 源码路径已审 |
| backend.actor_cache | SHP/VXL素材与姿态缓存 | SHP素材/姿态；VXL身体与炮塔/HVA/recoil；瞬态动画/particle；活跃与显示fallback姿态保留；资源/身份/光照变化淘汰 | 源码路径已审 |
| backend.cpu_frame_reuse | CPU已完成场景帧复用 | 输入相同；遮罩/效果/调色板/深度变化 | 源码路径已审 |
| backend.gpu_frame_reuse | GPU已完成场景帧复用 | 输入相同；呈现失效；CPU回退 | 源码路径已审 |
| backend.gpu_failure | Vulkan后端故障回退 | 设备/loader/能力不可用；预算与参数拒绝；提交/readback失败；100ms fence超时/设备丢失；host SEH/C++故障；CPU有序回放与进程级隔离 | 源码路径已审 |
| backend.xbrz | 保留的 xBRZ 缩放后端 | 当前产品 effectiveRenderScale()=1：最近邻，无 xBRZ 重建；保留后端 CPU xBRZ 2/3/4×；保留后端 Vulkan xBRZ 2/3/4×；分数倍率解析到实际尺寸；GPU 不可用/失败后的 CPU 回放 | 源码路径已审 |
| backend.live_shroud | 实景遮罩/可见性 | 跟随可见性/浓雾；揭示模式；当前视口采集；全图遮罩节流；CPU/GPU有界生成 | 源码路径已审 |
| backend.overlay_compositor | 统一覆盖层合成 | 主信息+主屏统计；分开窗口；隐藏/尺寸变化 | 源码路径已审 |
| backend.watch_slots | 三副相机槽位操作 | 设置/跟踪；取消；单击跳转；手动关闭 | 源码路径已审 |
| backend.watch_drag | 副相机组拖动/布局 | 主屏；副屏；DPI/尺寸变化 | 源码路径已审 |
| backend.map_navigation | 小地图点击/拖动定位 | 示意；实景；主屏；副屏 | 源码路径已审 |
| backend.map_drag | 小地图拖动/缩放位置 | 主屏自定义数值位置；副屏拖动/缩放；工作区/分辨率变化 | 源码路径已审 |
| backend.feedback | 主视图原生交互反馈重绘 | 拖动选择框；选中对象边角与血条；普通攻击/移动命令线；原生放置格和必要效果的精细透出；原生软件光标镜像；局部图层合成及失败撤回 | 源码路径已审 |
| backend.local_ui | 原生值驱动的独立本地 UI 层 | 消息/编辑文本；任务/超武/停电计时；主要工厂/电力状态/军衔/组号/维修/pips；集结与规划线；规划高亮 | 源码路径已审 |
| backend.lifecycle | 渲染任务生命周期 | 关闭/暂停/菜单/失焦；PID/进程重启/换局；视口尺寸/DPI/资源/profile变化；异步generation失效；关闭等待worker退出 | 源码路径已审 |
| neural.probe | 神经渲染运行时探测 | helper --probe；缺DLL；不支持设备/驱动 | 源码路径已审 |
| neural.ipc | 神经渲染IPC协议 | 有效输入；非有限数；越界；过期帧 | 源码路径已审 |
| neural.protect | 神经渲染保护区域 | 全保护跳过；局部保护保持原像素 | 源码路径已审 |
| neural.history | 神经渲染尺寸/历史 | 尺寸变化；generation变化；ResetHistory | 源码路径已审 |
| neural.cancel | 神经渲染取消关闭 | UI取消；父进程退出；job关闭 | 源码路径已审 |
| neural.fallback | 神经渲染故障隔离 | 初始化失败；GPU超时；客户端超时；隔离至重置 | 源码路径已审 |
| neural.cpu_transfer | 神经渲染CPU转换/传输 | BGRA→FP16；上传/回读；非连续映射缓存同步 | 源码路径已审 |
| cursor.animation_inference | 未知动画表的连续帧形状推断 | 正常连续帧；断帧/跳帧；同帧高频调用；原始SHA替换；55/128/256/512画布 | 已修复；受控验证通过 |

## 科技、硬件入口、地图、超武与诊断（82 条审查记录）

[完整机器可读明细](evidence/feature-point-audit-20261108/hardware-matrix.json)

| ID | 功能或机制 | 模式 / 范围 | 审查状态 |
| --- | --- | --- | --- |
| technology-mode-0 | 科技解锁：受控 | 0；开启/关闭/切换/重连 | 源码路径已审 |
| technology-mode-1 | 科技解锁：经典兼容 | 1；开启/关闭/切换/重连 | 源码路径已审 |
| technology-mode-2 | 科技解锁：直接 | 2；开启/关闭/切换/重连 | 源码路径已审 |
| technology-mode-3 | 科技解锁：原生侧栏规则数据 | 3；开启/关闭/切换/重连 | 源码路径已审 |
| technology-mode-4 | 科技解锁：原生侧栏参数拦截 | 4；开启/关闭/切换/重连 | 源码路径已审 |
| technology-search | 权限条目搜索 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-permission-selected | 切换所选科技许可 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-permission-all | 全部科技许可 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-permission-clear | 清空科技许可 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-permission-preset | 兼容科技预设 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-permission-view | 选择 Assist.ini [tech] 自定义许可 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-produce | 生产所选科技 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-cancel-production | 取消所选生产 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-production-status | 生产状态查看 | 受控/经典兼容/直接/数据/参数模式适用性 | 源码路径已审 |
| technology-enhance-0 | 科技加强：规则数据 | 0；开/关/世界切换/恢复 | 源码路径已审 |
| technology-enhance-1 | 科技加强：精确科技等级（参数断点） | 1；开/关/世界切换/恢复 | 源码路径已审 |
| technology-enhance-compare | 精确科技等级：比较玩家科技等级 | 关/开（另占1槽） | 源码路径已审 |
| hardware-slots | 硬件断点4槽预算、所有权和异常派发 | 外部槽/不足/签名拒绝/部分失败/恢复 | 源码路径已审 |
| placement-mode-0 | 建筑放置：受控 | 0；开启/关闭/切换 | 源码路径已审 |
| placement-mode-1 | 建筑放置：直接 | 1；开启/关闭/切换 | 源码路径已审 |
| placement-mode-2 | 建筑放置：原生事件 | 2；开启/关闭/切换 | 源码路径已审 |
| native-placement-product | 匹配当前栏目已完成建筑 | 建筑/防御栏目；原生事件模式 | 源码路径已审 |
| native-placement-preview-key | 切换自动放置预览 | 建筑/防御栏目；原生事件模式 | 源码路径已审 |
| native-placement-place-click | 预览后左键放置 | 建筑/防御栏目；原生事件模式 | 源码路径已审 |
| native-placement-cancel | 取消预览 | 建筑/防御栏目；原生事件模式 | 源码路径已审 |
| control-mode-0 | 控制全部：硬件参数 | 0；开/关/跨玩家/重连 | 源码路径已审 |
| control-mode-1 | 控制全部：原生Hook | 1；开/关/跨玩家/重连 | 源码路径已审 |
| control-mode-2 | 控制全部：Master三处补丁 | 2；开/关/跨玩家/重连 | 源码路径已审 |
| control-command-owner | 命令采用单位所属方 | 控制关闭/原生/硬件/Master适用性 | 源码路径已审 |
| control-network-house | 保留网络事件所属方 | 控制关闭/原生/硬件/Master适用性 | 源码路径已审 |
| control-native-sell | 原生出售光标 | 控制关闭/原生/硬件/Master适用性 | 源码路径已审 |
| control-destruct | 自毁所选单位所属方 | 控制关闭/原生/硬件/Master适用性 | 源码路径已审 |
| control-switch-disabled | 切换当前控制玩家（暂不可用） | 控制关闭/原生/硬件/Master适用性 | 禁用门槛已审 |
| map-mode-1 | 黑幕绘制：半透明绘制 | 1；关闭/局切换/失败恢复 | 源码路径已审 |
| map-mode-2 | 黑幕绘制：透明绘制 | 2；关闭/局切换/失败恢复 | 源码路径已审 |
| map-mode-3 | 黑幕绘制：原生自动揭示 | 3；关闭/局切换/失败恢复 | 源码路径已审 |
| map-mode-4 | 黑幕绘制：直接全图 | 4；关闭/局切换/失败恢复 | 源码路径已审 |
| map-manual | 手动地图揭示许可和执行 | 允许/禁止/快捷键/排队 | 源码路径已审 |
| map-hardware | 硬件黑幕显示 | 开/关/与其他揭示模式互斥 | 源码路径已审 |
| crc-shadow | CRC原始MapIsClear影子 | 开/关/晚启用/直接全图/局切换/恢复 | 源码路径已审 |
| sw-per-frame | 逐观察帧永久授予所有已注册超武 | 开/关/重复帧/充能保留/已授予保留 | 源码路径已审 |
| sw-panel | 独立超武面板开启与状态 | 开/关/刷新/目标选择/取消 | 源码路径已审 |
| sw-move-fold | 超武面板拖动折叠 | 开/关/刷新/目标选择/取消 | 源码路径已审 |
| sw-target | 面板超武激活、目标光标与取消 | 开/关/刷新/目标选择/取消 | 源码路径已审 |
| single-player-fps | 自定义单机FPS | 0原版；10–240；非法值；联机拒绝 | 源码路径已审 |
| pause-toggle | 单机暂停/恢复 | 关/开；F5；失焦/联机/退出恢复 | 源码路径已审 |
| pause-step | 暂停后前进一帧 | F6；单步排队/取消 | 源码路径已审 |
| native-mega | 原生任务成功通知入口 | 开/关；调用成功/失败；签名拒绝 | 源码路径已审 |
| native-combat | 精确原生战斗查询 | 开/关；ABI/签名/访问失败 | 源码路径已审 |
| frame-maintain | 帧通知诊断维护热路径 | 普通消息无诊断；延后开启；关闭；换控制器；死亡；恢复失败；策略变更 | 已修复；受控验证通过 |
| diagnostic-step-ReadOnly | Hook诊断步骤：ReadOnly | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-RuntimeOnly | Hook诊断步骤：RuntimeOnly | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-FrameSignal | Hook诊断步骤：FrameSignal | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Tooltip | Hook诊断步骤：Tooltip | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Mega | Hook诊断步骤：Mega | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Placement | Hook诊断步骤：Placement | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Camera | Hook诊断步骤：Camera | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-LowDamage | Hook诊断步骤：LowDamage | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-AirType | Hook诊断步骤：AirType | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-AirFull | Hook诊断步骤：AirFull | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Control | Hook诊断步骤：Control | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-ShroudClear | Hook诊断步骤：ShroudClear | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-ShroudHalf | Hook诊断步骤：ShroudHalf | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-TechPair | Hook诊断步骤：TechPair | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-TechMaster | Hook诊断步骤：TechMaster | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-DirectPlacement | Hook诊断步骤：DirectPlacement | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Network | Hook诊断步骤：Network | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-CommandOwner | Hook诊断步骤：CommandOwner | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Sell | Hook诊断步骤：Sell | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-Crc | Hook诊断步骤：Crc | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-step-DirectMap | Hook诊断步骤：DirectMap | 单项；从此继续；正常/异常/跳过；恢复 | 源码路径已审 |
| diagnostic-exclude | Hook诊断：排除可疑组及共享依赖 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-skip | Hook诊断：跳过当前组并恢复 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-stop | Hook诊断：结束诊断并恢复 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-continue | Hook诊断：已恢复后继续下一组 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-focus | Hook诊断：失焦暂停观察与焦点恢复确认 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-oos | Hook诊断：OOS/断线停止 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-persist | Hook诊断：排除组持久化 | 操作有效/无效阶段；错误处理 | 源码路径已审 |
| diagnostic-report | Hook诊断：诊断观察记录 | 操作有效/无效阶段；错误处理 | 源码已修；未单独运行该路径 |
| sw-iron-hotkey | 铁幕独立快捷键 | SelectIron → IronCurtainSpecial；前台/非聊天/当前对局/过期拒绝 | 源码路径已审 |
| sw-chrono-hotkey | 超时空独立快捷键 | SelectChrono → ChronoSphereSpecial；前台/非聊天/当前对局/过期拒绝 | 源码路径已审 |
| sw-custom-hotkey | 按超武类型ID绑定快捷键 | SelectSuper via current snapshot ID→native index；前台/非聊天/当前对局/过期拒绝 | 源码路径已审 |

## Host、控制台、配置与共享机制（44 条审查记录）

[完整机器可读明细](evidence/feature-point-audit-20261108/host-matrix.json)

| ID | 功能或机制 | 模式 / 范围 | 审查状态 |
| --- | --- | --- | --- |
| host.bootstrap | 首次启动建立默认配置 | 新目录；已有配置；写入失败 | 源码路径已审 |
| host.single_instance | 单实例与再次打开窗口 | 首次启动；重复启动 | 源码路径已审 |
| host.module_enable | 光标与单位监视模块开关 | 关闭；开启；启动中；服务意外退出 | 源码路径已审 |
| host.navigation | 设置分类与子页面切换 | 六类设置；视频子页；会话刷新 | 源码路径已审 |
| host.bool_input | 复选框操作 | 开启；关闭；不可用 | 源码路径已审 |
| host.number_input | 数字设置编辑 | 整数；滑条；范围边界；未提交草稿 | 源码路径已审 |
| host.text_input | 文本设置编辑 | 普通文本；长度上限；未提交草稿 | 源码路径已审 |
| host.choice_input | 列表与下拉项选择 | 单选列表；组合框；选项变化 | 源码路径已审 |
| host.hotkey_capture | 快捷键编辑与捕获 | 按下；松开；按住；双击；清除 | 源码路径已审 |
| host.immediate_action | 立即执行按钮与新鲜度 | 立即动作；延迟到达；过期动作 | 源码路径已审 |
| host.search_groups | 当前设置搜索与分组折叠 | 筛选；折叠/展开；含错误分组 | 源码路径已审 |
| host.virtualized_controls | 长设置页渲染 | 普通分组；长列表延迟呈现 | 源码路径已审 |
| host.diagnostic_view | 控制台诊断筛选与过期状态 | 筛选；文本检索；数据过期 | 源码路径已审 |
| host.action_queue | 前后端命令排队与确认 | 正常；队列满；页面更换；owner 销毁 | 源码路径已审 |
| host.snapshots | 设置快照一致性 | 增量更新；快速切页；浏览器重建 | 源码路径已审 |
| host.webview_document | 内嵌控制台消息边界 | 初次加载；重建；异常导航 | 源码路径已审 |
| host.webview_failure | 控制台加载失败恢复 | 缺少运行库；加载失败；重试 | 源码路径已审 |
| host.ini_single | 普通设置保存 | 新增/覆盖键；删除键；删除节；flush | 源码路径已审 |
| host.ini_section | 整节设置保存 | 完整替换；随后单键修改；同节连续替换 | 源码路径已审 |
| host.ini_failures | 保存失败提示与重试 | 本地 OS 失败；队列/缓存拒绝；重试；退出中 | 源码路径已审 |
| host.close_drafts | 关闭前处理未提交设置 | 无草稿；有草稿；前端无响应；再次关闭 | 源码路径已审 |
| host.close_saves | 关闭前完成保存 | 正常保存；保存失败；队列繁忙；用户再次关闭 | 源码路径已审 |
| host.close_cleanup | 关闭时清理模块和后台工作 | 正常完成；owner 卡住；消息循环异常退出 | 源码路径已审 |
| host.process_discovery | 自动发现游戏进程 | 已知文件名；描述关键字；记住的程序 | 源码路径已审 |
| host.process_search | 搜索与刷新候选进程 | 筛选；刷新；目标退出；枚举失败 | 源码路径已审 |
| host.process_keywords | 修改自动识别关键字 | 保存成功；保存失败 | 源码路径已审 |
| host.process_remember | 记住或移除程序规则 | 添加运行程序；添加路径；移除；重复规则 | 源码路径已审 |
| host.process_manual | 手动指定或清除连接目标 | 指定进程；清除指定；目标重启 | 源码路径已审 |
| host.process_status | 连接状态与操作可用性提示 | 未连接；只读；等待 Runtime；可发送命令 | 源码路径已审 |
| host.input_binding | 运行期按键手势匹配 | 按下；松开；按住；双击按住 | 源码路径已审 |
| host.pointer_capture | 鼠标快捷操作采样 | 中键；侧键；左键；右键 | 源码路径已审 |
| host.pointer_cancel | 视口和手动输入取消 | 右键拖动；切前台；最小化；比赛/视口切换 | 源码路径已审 |
| host.click_delivery | 合成点击安全配对 | 完整点击；只发送按下；窗口改变 | 源码路径已审 |
| host.shared_wake | 共享 Runtime 唤醒 | Cursor；Tactical；共同订阅；PostMessage 失败 | 源码路径已审 |
| host.target_lease | 目标身份与订阅释放 | 多订阅；窗口重建；进程退出；卸载失败 | 源码路径已审 |
| host.broker_startup | x64 Host 启动 x86 Broker | 首次启动；响应慢；超时 | 源码路径已审 |
| host.broker_recovery | Broker 丢失后的重连 | 健康；Broker 退出；延迟恢复；恢复失败 | 源码路径已审 |
| host.broker_backoff | Broker 失败退避和资源退休 | 超时；安静期；重复回收 | 源码路径已审 |
| host.health_snapshot | Runtime 诊断状态一致性 | 稳定快照；并发更新；暂时读不到 | 源码路径已审 |
| host.overlay_shared_key | 多个覆盖层共用显示快捷键 | 单模块；两个模块；前台变化；owner 交接 | 源码路径已审 |
| host.replaceable_work | 最新结果型后台工作 | 新请求替换待执行；执行中失效；停止 | 源码路径已审 |
| host.recurring_work | 定期后台工作 | 更新输入；租约到期；停止/重建 | 源码路径已审 |
| host.ui_fairness | 界面消息与后台消息公平处理 | 输入；绘制；定时器；普通消息 | 源码路径已审 |
| host.critical_log | 关键错误日志 | 路径可写；日志轮换；主路径失败 | 源码路径已审 |

## 跨组实现核对

战术组中原有四个底层委派项，已单独追踪 native 低伤害、两种 native 飞机路径及 Host 生产预定状态机。不能把 Hook 诊断配置步骤等同于底层功能实现。

[四项交叉核对记录](evidence/feature-point-audit-20261108/delegated-coverage-crosscheck.json) · [生产预定跨世界边界复核](evidence/feature-point-audit-20261108/reservation-world-boundary-review.json)
