# 1.0.52-pre.18 功能采样清单

原始表格记录 pre.18 旧策略。2026-11-23 起执行位置以本段修订为准：所有会产生游戏操作的规划与自动功能均由 Runtime 游戏线程采样、计算和提交；Host 只负责小地图、副相机、覆盖层及预定面板等只读显示。Runtime 在下令前重新核验 UID、归属、任务和当前合法性。完整世界 CommandCaptureWorker 默认关闭，AsyncPublisher 只传输已封存样本并提供有限缺页提示。

| 功能 | 样本和计算路径 | 提交或缺样处理 |
| --- | --- | --- |
| 同类、闲置和组合选兵，选区显示 | 封口选区与 Runtime UID 索引。Host 已移除周期性实时选区指纹，以样本和界面配置去重。 | 显式选择输入及原生操作仍核验当前选中资格。 [选择](../assist/source/type_select_bridge.h) · [信息显示](../assist/source/main.cpp) |
| 高级移动攻击、磁电轮换、全局吸和摔落、防偷 | 候选单位、目标和战斗事实由 Runtime 游戏线程的完整采样和索引提供，排序、规划及操作准入均在 Runtime。 | Runtime 校验人工命令版本和当前合法性。原生武器询问留在游戏线程。 [规划](../assist/source/external_orders_bridge.h) · [命令](../assist/source/native_orders_bridge.h) |
| 高级进驻、飞机分配、自动装车、绑弹上车 | 源、目标、容量及到途名单由 Runtime 同帧样本提供。进驻、飞机配对、装车和绑弹配对均在 Runtime 游戏线程。 | 按入口等待同帧或使用受限跨帧证明。已坐、到途和已接受预留共同占用容量。 [协同](../assist/source/cooperative_bridge.h) · [装车](../assist/source/autoload_bridge.h) · [绑弹](../assist/source/bomb_board_bridge.h) |
| 智能卸载、载员逃生 | 同源乘员历史和下车后候选。 | 原生卸载与工具箱续令分开。缺样时保留请求，具体载具、成员和目标经实时确认。 [卸载](../assist/source/native_orders_bridge.h) · [续令](../assist/source/unload_compute_bridge.h) |
| 自动或一键维修，倒矿、防蜘蛛 | 样本提供受损建筑、矿厂、矿区和威胁。人为暂停的矿车也从封口索引观察任务。 | 缺页不解除人工状态；入队前核对持有者、对象身份和当前任务。 [维修](../assist/source/bridge.cpp) · [倒矿](../assist/source/ore_harvest_bridge.h) · [防蜘蛛](../assist/source/harvester_defense_bridge.h) |
| 躲弹、倒车躲避、导弹拦截 | 弹体、地形及目标来自统一样本，默认 Runtime 计算。 | 提交前检查原生命令，缺样保留已拥有的任务。 [躲弹](../assist/source/dodge_bridge.h) · [倒车](../assist/source/reverse_dodge_bridge.h) · [拦截](../assist/source/missile_bridge.h) |
| 模糊攻击、区域分配、编队、巡逻及飞机路径点 | 区域的格子、对象、距离和分组条件来自一份封口样本。编队和路径保留鼠标输入、网格意图和原生编辑状态。 | 区域缺样保留松键操作和预览轮廓。原生动作查询、座位核对及区域入队仍要当前索引。高逻辑帧率不再触发旧的固定帧数取消。 [手势](../assist/source/mo_orders_bridge.h) · [编队](../assist/source/formation_bridge.h) · [飞机路径](../assist/source/air_path_host.h) |
| 生产预定、类型目录、科技加强 | 预定用资源样本里的工厂、在制及前置科技事实。类型重建和科技加强的类型发现绑定采样 Worker 的类型资料。 | 重复旧样本不推进放置观察或累积失联时间。工厂和当前建造图标在待提交时复核。类型目录完整后一次更新，规则切换等待较新的样本。 [预定](../assist/source/construction_host.h) · [采集](../shared/source/world_frame_capture.h) · [科技加强](../assist/source/tech_ui_breakpoint_bridge.h) |
| 主屏、单位监控和覆盖层、实景小地图、合成副相机 | 小地图、副相机、覆盖层和预定面板显示由 Host VisualCache 独立只读采样，允许短时缓存和轻微延迟；显示数据不作为任何命令的授权依据。 | 缺页等待或保留完整的同局画面。窗口、视口、点击命中和素材文件属于局部输入和资源。 [场景](../external_reader/source/shared_scene.h) · [信息层](../external_reader/source/tactical_sampler.h) · [监控](../unit_monitor/source/sampling_task.h) |

## 原生执行及其理由

| 入口 | 无需工具箱进行世界规划的原因 |
| --- | --- |
| 驻守、停止、散开、部署、超武选择 | 玩家已经指定单位或槽位。动作语义和合法性由原生任务及超武系统决定。Runtime 只核验当前身份与权限，再执行原生动作。 |
| 原生放置、变卖光标、雷达跳转、视角或侧栏切换、原生画中画 | 当前点击、位置和界面已经提供输入。游戏本来就负责实际命中、布局及绘制，Runtime 使用当前的局部对象和视图。 |
| 原生路径点编辑及最后提交 | 游戏编辑器持有玩家给出的节点和原生计划，按原先规则产生事件。自定义巡逻路线生成、区域目标搜索和智能续令仍包含工具箱阶段。 |
| 单机限速、暂停和逐帧，剧情输入与镜头保护 | 原生循环、输入锁和相机入口消费配置。没有单位候选或分配需要计算。 |
| 科技/控制权原生判定、生产许可、授予超武、建造提示与原生闪烁 | 原生执行依赖当前 House、在制品、合法性或正在绘制的图标。工具箱的候选目录、显示和预测使用样本。 |

## 保留的边界

- 所有会产生操作的规划均在 Runtime 游戏线程执行；原生调用前仍需重新读取 UID、对象归属、任务和当前合法性。允许这些及时性关键路径占用游戏线程，不再为了全面异步而增加跨进程操作往返。
- 当前选区的显式手势、原生点击命中、视口、光标、素材及签名只需要局部输入或资源数据。
- 无 Runtime 的独立只读监控仍有独立观测。旧 Runtime-only 显示也保留其兼容入口，正常集成控制台启用 Host 显示。
- 自动 CS、武器变体选择、旧步兵强制进入及强制刷弹沿用暂挂状态。相关可用性开关见 [功能标记](../shared/source/feature_availability.h)。
- 同帧证据不足时可从完整的候选样本发现目标。区域下令提交、批量容量和状态清退按各自准入条件等待。实机高 FPS、延迟及联机行为尚需对局测量。

## 回归入口

- [统一采样与发布](../assist/tests/world_frame_core_test.py) · [Runtime 同帧索引与 Host 显示分流](../assist/tests/runtime_action_display_split.py) · [生产现场名单检查](../assist/tests/async_command_no_live_roster.py)
- [手势缺样及提交](../assist/tests/mo_orders_bridge_test.py) · [预定队列](../assist/tests/construction_host_test.py)
- [科技目录与原生准入](../assist/tests/tech_enhancement_midgame.py) · [既有采样审计](2026-11-20-pr71-feature-sampling-audit.md)
