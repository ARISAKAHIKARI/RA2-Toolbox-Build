# 2026-11-16 · 缺页/未知状态的全消费链审计（仅状态机，不迁移计算架构）

## 边界

本轮审查 **封口 SampleReadScope / Host WorldFrameScope 消费者、延迟 Host 结果、关键任务状态机及显示提交入口**。固定的 Host/Runtime 计算分工保持不变；不增加游戏 Hook、Native 查询路径、事件组包或跨进程采样频率。以下的“未知”包含三种不同问题：

1. 封口样本不含必需字段（missing page / future demand）；不是 Windows 物理缺页异常。
2. 页已请求但实际不可读、读取不完整；必须区别于合法的 0、空名单、死亡。
3. 快照或异步结果与当前比赛、帧、UID、手动版本不匹配；不得用后续绘制刷新源时间戳。

只有**完整有效的观测**能够推进命令任务、退休手动保护或发布显示状态；**明确不属于本局/实体、手动版本变化、用户关闭功能**仍可取消。读失败一律不能触发不可逆清理；命令最终仍需要原生实时许可与出队回执。

## 实际修复（PR #36 同一分支）

| 路径 | 已确认的旧错误 | 本轮处理 |
| --- | --- | --- |
| `shared/source/world_frame.h::Snapshot::read`（上一阶段） | 请求跨多页时后页缺失，读函数返回 false 前已经复制前页到调用者输出 | 跨页先验证全部范围，失败输出一字节不写；单页保留快速路径 |
| `ore_harvest_bridge.h::oreHarvestTick` | 资源搜索完成后的矿车身份/Cell 二次读取失败仍进入 admission，按普通负回执耗尽 3 次重试，丢掉自动采矿恢复任务 | 引入当场三态：未知保持搜索结果与重试数；身份确定失效才退休；资源确认耗尽重新搜索；恢复后重验再下令 |
| `harvester_defense_bridge.h::minerDefenseTick` | `valid==0`、source/target/transition 不可读超过 2s 会直接释放防御任务和租约 | 未知期间保持任务/原始手动版本，下一次可靠观测再判断；不同于确认身份失效 |
| `harvester_defense_bridge.h::minerDefenseResume` | `minerDefenseSource` 读取失败返回 true，调用方将其解释成“恢复已完成”，错误删任务 | 失败返回 false；保持恢复阶段，后续重试 |
| `harvester_defense_bridge.h` 炼厂决策 | `oreHarvestRefinery==-2`（不可读）与 `-1`（无效）合并，可能清掉原来的炼厂身份并切 Guard | 三态区分，未知保留原炼厂/电台链接，暂不下新命令 |
| `harvester_defense_bridge.h` 搜索游标 | 恢复采矿时在确认 Cell 结果前原位推进 search，后面的缺失字段会留下部分观察 | 先推进局部搜索副本，只有所有访问确定后提交游标 |
| `harvester_defense_bridge.h` 当前帧准入 | 初始化 `minerDefenseFrame` 早于炼厂名单完整性判断，单帧缺页可锁死本帧再次检查 | 完整 index 后才提交处理帧戳；保留同帧有界补读机会 |
| `harvester_defense_bridge.h::minerDefenseVictim` | 敌方目标类型、血量、类型 ID 或坐标不可读时作为“不满足威胁”，可能提前回采矿 | 给这些显式读取引入 unknown 标志；正在攻击的任务保留，不提前进入 restore |
| `missile_bridge.h::pruneMissileManual` | 手动指令 roster 在检查完成前标记已检查该帧；某个 UID 页失败后同一帧无法补查 | 完整快照核验之后再提交 frame stamp；在失败帧保留全部手动保护记录 |
| `missile_bridge.h::missileManuallyOwned` | 未知任务/目标/目的地的 `get` 默认值 0 可被误识别为玩家手动操作结束 | 单独检查 UID/mission/queued/target/destination；任何必需读取不可用都保持手动命令优先权 |
| `bomb_board_bridge.h::collect` | 候选 `armed/vehicles` 在验证 sealed sample 之前被直接清空并增补 | 用可复用候选容器暂存，样本完整才 swap 发布；失败不污染上一完整名单 |

## 逐类消费审查清单

此表区分 **已检查代码边界** 与 **真实游戏验收**，不是声称“所有功能在实机 100% 无 bug”。

| 功能 / 链路 | 代码完整性与提交边界 | 审查状态 |
| --- | --- | --- |
| Runtime 世界身份/手动版本 | `indexWorld` 只有整个 UID roster 可读才发布；命令侧 `refreshWorldIndex(executionGuards)` 缺页后只补**实时身份**，失败保留任务；显示不补 live | 已审查、保留 |
| 自动倒矿 / 卸矿恢复 | `oreHarvestObserve` 缺页回滚观测/待执行任务；`oreHarvestTick` search 事务提交；补齐完成搜索后的二次读三态 | 已修、生产路径合成内存测试 |
| 点杀蜘蛛与采矿/守卫恢复 | 原子炼厂名单、目标存活、任务状态及恢复证明；缺页不再按 2s 清理 | 已修、生产路径合成内存测试 |
| 自动装车和续发 | `collectLoadFacts` 完整样本才提供候选；当前命令准入与在途账本仍有即时 live 验证 | 已审查；live 账本删除的读取完整性仍须继续加固 |
| Ctrl+D 智能卸载及乘员后续命令 | `collectUnloadTargets`、`collectUnloadSource` 均分阶段提交，保留 UID/manual 回执；Runtime 执行守卫已有缺样本 live identity fallback | 已审查；多乘员场景仍需实机核验 |
| 飞机分蛋 / 高级进驻 | `collectCooperativeInput` 完整性检查后才发布，外部结果需匹配请求帧和身份 | 已审查 |
| 高级移动攻击、磁电、返矿规划 | `external_orders_bridge.h` 纯世界候选遇缺页回滚 weapon cache；最终 admission 仍 live 原生权限和回执 | 已审查 |
| 反蜘蛛防御 / AntiTheft | `anti_theft_sample_bridge.h` 缺页不提交负缓存；Runtime Combat 依赖已显式声明 | 已审查 |
| 防空拦截、刷弹 | `collectMissileInput` 失败回滚 motion / weapon cache；本轮加固手动名单与实时手动任务证明 | 已修、手动状态合成测试 |
| 自动躲弹 / 顺向躲弹 | Bullet/Combat 需求、`collectDodgeThreats` 局部候选、`collectDodgeInput` 完整才发布；Host result 帧次校验 | 已审查；持续运行任务的**实时** `dodgeSource` 假阴性仍需单独故障夹具 |
| 炸弹步兵自动上车 | `bomb_board_bridge.h` 采集候选改为完整后交换；已有任务只在成功采样后淘汰 | 已修；尚缺针对该入口的动态缺页注入 |
| 游戏内部分主动攻击/维修 | 候选和最终原生验证分离，repair defer 不消耗间隔，未读成功不发布 source | 已审查共享采样入口；需运行期性能和实机校验 |
| Host 共享世界 + Unit panel | Host `sampleExternal` 与 `tactical_sampler` 校验 `WorldFrameScope.complete()` 后发布；Cameo 素材读取与命令状态隔离 | 已审查 |
| 载员气泡/敌方标记 | 显示样本携带比赛/帧/投影 key；失败只有限期复用旧完整画面，不授权输入 | 已审查 |
| 飞机路径点 | Host `air_path_host.h` 观察组必须 `source.complete()` 才推进游标和任务；实际提交仍核验当前单位身份 | 已审查 |
| 建筑预定、生产面板 | `construction_host.h::collectChoices` 对 native 校验失败和封口缺页恢复上一份 choices；异步 Paint 需要完整场景和匹配 key | 已审查 |
| 小地图、副相机、渲染 | `minimap_overlay`, `render_upgrade_host`, `shared_scene` 都在发布前验证来源及完整性；渲染失败不许“更新”原始控制样本时间 | 已审查；实际丢帧仍需平台/性能诊断 |
| 侧栏状态及特殊单位叠加 | `sidebar_indicator_host` 的样本在 `worldFrameComplete()` 后提交；`special_unit_overlay_host` 完整性与世界当前性同时校验 | 已审查 |
| 队形预览 | `formation_host` 将来源 Token 绑定异步帧，完成结果携带原始请求身份；渲染是只读观察 | 已审查 |

## 未宣称解决的风险与后续严格边界

1. `autoload_bridge.h::autoLoad/autoLoadTick` 在**封口样本域之外**仍根据 live `get/own` 的否定结果清理 ledger。这里 `get<T>` 在 Runtime live 模式下是本进程指针读取，失败通常不是可恢复的封口缺页，而可能是对象生命期/异常。需要专门的对象瞬间退出/重用测试和读取成功标志，不能用旧世界数据盲目继续发单。
2. `dodge_bridge.h::updateDodgeJobs` 的 live `dodgeSource` 为假会取消活跃闪避租约；需要区分已证实原生失效与读取不确定。现有 `collectDodge*` 纯样本域已原子，仍需另一组 live 故障测试。
3. `minerDefenseVictim` 本轮对显式 `oreHarvestRead` 检测了 unknown；`minerDefenseThreat/rawCombatInfo` 内更深层的原生字段读取仍要通过精准缺页夹具证明确认。
4. 主动 `nativeMission` 最终许可失败不能伪装成缺页；保留回执、手动优先级、原始 deadline，禁止无限重放。失效时间基准仍以原始有效样本为准。
5. 所有测试仅为生产源码+合成内存/ABI 边界；尚未运行 Windows 实际游戏、多平台双机联机/FPS 验收。**不合并/不发布**直到对应 CI 和实机门槛明确。

## 回归测试与持续门禁

- `source/assist/tests/world_frame_core_test.py`：真实 worldframe 捕获、封口、缺页、atomic 跨页读、Host 隔离。
- `source/assist/tests/tactical_frame_sampling.py`：Ctrl+D 身份遗漏、Ore 事务式 checkpoint、Host/Runtime 决策保留等生产路径。
- `source/assist/tests/harvester_defense_runtime.py`：蜘蛛攻击→恢复，以及数秒未知、资源 search、炼厂读失败、被动单位、手动取消、UID 复用。
- `source/assist/tests/ore_harvest_handoff.py`：真实自动返矿观察/Native admission，矿车与 Cell 连续缺页不得吃光重试。
- `source/assist/tests/missile_manual_lifecycle.py`：玩家手动任务保护、同帧坏页重试、UID/mission/queued/target/destination 缺字段保持优先权。
- `.github/workflows/validate-automatic-compute.yml`：独立 `regression`、`defense-state`、`ore-state`、`missile-state` 作业，避免一个旧夹具失败导致另几项未执行。

受测内容与运行报告按准确提交 SHA 关联；不得把单元通过表述成实机成功。当前 PR: https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/pull/36
