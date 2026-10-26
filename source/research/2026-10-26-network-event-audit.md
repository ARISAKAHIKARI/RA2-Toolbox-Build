# 事件投递、原版同步摘要与飞机联机实验（2026-10-26）

## 基线与证据

源码基线为 main / v1.0.38，提交 `9c6509a2a6cf88f95af14834acbd90d0e2d36da1`。本分支增加 1.0.39-pre.1 飞机实验选项；正式版的默认联机兼容行为保留。本文记录新发现，不回填旧研究。

原版样本 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。本次读取用户已有捕获中的 `00-clean-game.mem/.valid`，对原版有效指令做反汇编和受控 x86 执行。映射图像、重建测试 PE 与原 EXE 不是同一个文件，不能混用文件哈希。私有 EXE、映射图像和原始反汇编不发布。

证据分为：源码审计；原版指令受控执行；编译后 Runtime 的合成 Win32/世界测试。**没有本次 Windows 实机操作、完整寻路/到达或真实双机联机通过的证据。**

## 构造、发送与执行是三个步骤

`assist/source/bridge.cpp` 的 `submitConstructed` 调用原版构造器填充临时 Event。构造器本身不传播事件。统一 `enqueue` 根据会话路由：

- LAN/Internet，Session=3/4：OutList `0xA802C8`，111 字节 Event 包、队列时间戳、计数和尾指针一起更新。
- Campaign/Skirmish，Session=0/5：DoList `0x8B41F8`，包起点 `0x8B4204`，调度到下一帧。
- 不支持的会话拒绝。构造成功、入队成功、实际执行成功是不同证据。

另两种发送后端仍是联机发送：`QueueMegaMission 0x646E90`；`Techno::ClickedEvent 0x6FFE00`。源码核对其是否真实追加 OutList，避免重复入队。原版函数直调不能一概视为本地执行。

当前活跃构造类型为 MegaMission=04、Idle=06、Scatter=07、Produce=0E、Abandon=10、SpecialPlace=12、Repair=15、Sell=16、PlanCommit=2B、AllCheer=2D。目录中的 Suspend、AbandonAll 等不等于活动功能：部分仅诊断探针放行；Place/SellCell/Waypoints 构造适配器未作为现有功能的实时发送器。人工放建筑等仍由游戏原生输入产生事件。

## 当前功能清单

表中“事件”均指联机会话最终经 OutList；工具本机计算不等于本机执行战局。

| 功能 | 实际实现 | 额外本机修改 |
| --- | --- | --- |
| 自动装车 | MegaMission Enter=7 | 本地距离、容量、分配，不直接写乘员 |
| 矿车倒矿 | 原版任务事件与移动事件 | 本地排队、避让，不写金钱 |
| 智能进驻/维修厂 | MegaMission Enter=7 | 本地候选和容量判定 |
| 自动维修 | Repair=15 | “不可修”只放宽工具候选过滤，不写 Repairable |
| 高级移动攻击 | MegaMission Move/Attack/原生 AttackMove | 本地重新选敌 |
| 自动 CS/自定义 CS | MegaMission Attack/AttackMove=29 | 当前活动路径无旧 CS 内联转换 |
| 磁电轮换 | MegaMission Attack | 本地轮换 |
| 磁电自动放下 | Idle=06 | 不直接清链或改坠落伤害 |
| 飞机分散攻击 | MegaMission Attack | 本地目标与配额 |
| 飞碟主武器 | 对格子发 Attack | 不交换武器指针 |
| 导弹拦截 | Attack/Sleep/Guard 任务事件 | 本地火力分配，不改导弹 HP |
| 躲弹 | Scatter=07 或 Move=2 | 本地计算方向 |
| 队形排列 | 各成员 Move=2 | 本地计算位置 |
| 战车/被吸住目标变卖 | Sell=16 | 工具放宽下单入口；最终效果取决于原版接收器 |
| 智能卸载 | Unload=16 及后续任务事件 | 本地规划，非完整游戏有效性认证 |
| 生产预定/取消 | Produce=0E / Abandon=10 | 本地预定和侧栏显示，不写 House/Factory 生产对象 |
| 超级武器 | SpecialPlace=12，或先启用原生目标光标 | 光标和 Display 状态是本机 UI |
| 喝彩 | AllCheer=2D | 本地冷却 |
| 嘲讽 | 注册的原版 Command::Execute 536350 | 原版通信发送，不是模拟 EventClass |
| 步兵强制进入 | MegaMission 规划追加 + PlanCommit=2B | 不安装 C4 能力补丁；原版继续判断 |
| 飞机 Z 兼容模式 | 本地记录/判断到点，再发送 Move | 不改原版到达逻辑 |
| 飞机 Z 原生模式 | 放开 CanUseWaypoint + 到达时推进原生规划 | 正式版仅单机；实验选项见后文 |
| Sticky 联机进入 | MegaMission Sticky=6 | 不直接写本机任务 |
| Sticky 单机进入/解除 | Idle 配合直接 ForceMission/NextMission | 直接改变本机任务 |
| 普通/经典科技 | 侧栏 CanBuild 调用条件 + Produce | 6A97D2/6AA781 本地侧栏调用点 |
| 大师科技 | 本地枚举/保留图标 + Produce | 44574A/445795/4457D0/445810/6AA788 |
| 科技加强 | 指定 21 型号 TechLevel=1 | 直接写 Type+634，非事件 |
| 随处建造 | 本机入口补丁，人工放置仍走游戏事件 | 受控 4A8EB0；直接 4A8EC3/4A918E |
| 控制其他玩家 | 放宽控制判定，按实际所属方发送任务 | 50B6F0/6339BA/637E40 本地补丁 |
| 地图绘制模式 | 黑幕/迷雾绘制补丁 | 47F038/47F274，区别于揭示地图状态 |
| 地图原生揭示 | 直接 Map::Reveal=577D90 | 写 House+240/+241 及地图揭示状态 |
| 地图直接模式 | 可见性/雷达等入口补丁 | 508E60/508EBA/508F8F/577B3A/6D88FC |
| 快捷选兵/空闲飞机 | 原生选择函数 | 本机选择状态 |
| 清除信标 | 431410(this=89C3B0,owner) | 显式 owner/slot 路径不发远端移除通知 |
| 统计、计时、气泡、标记、小地图、副视图、生产提醒、高刷鼠标 | 读取、工具渲染或本机显示调用 | 不通过模拟事件改变战局 |

残留但禁用：C4 能力覆盖 51A4D8/51C32B；切换 CurrentPlayer 上下文；敌方建筑强卖；场景刷新、视角缩放。不要仅凭定义存在就归类为活动补丁。另一个上传的 rlapi 的 place_mem/clean 不是当前仓库的生产实现。

## 科技全开为何可能兼容原版接收端

普通/经典科技只改变本地侧栏两个调用点的 `limitOnly` 参数；未覆盖整个 House::CanBuild。大师枚举所在原版 Building::UpdateConstructionOptions=4456D0 先检查 Owner==CurrentPlayer；图标刷新路径与一般战局生产判定分开。

实际 Produce 接收路径为：

`Event::Execute 4C711B → House::BeginProduction 4FA350 → Type::FindFactory 5F7900 → House::CanBuild 4F7870`。

4FA42C 调 FindFactory(false,true,true,house)。5F7990 再用 `CanBuild(type,limitOnly=true,includeProduction=true)`。CanBuild 在 4F787D 按 limitOnly 跳 4F7F9E，绕过包括 4F794A TechLevel==-1 在内的完整可用性路径；工厂、生产限制等仍有原版条件。

因此本机能从侧栏下单、未修改端也一致消费同一个 Produce 可以同时成立。科技加强直接写 TechLevel 也未必改变这一条 limitOnly 接收路径，但它是全局类型字段，其他消费者、AI、MOD 影响未证明。不能把这里的结论推广到所有判定补丁。

## 原版联机每帧摘要的精确输入与比较

实际函数 `ComputeGameCRC=64DAB0`，调用点 64731C/647684，结果 `AC51FC`。它不是 YRpp 通用 Checksummer 所使用的表驱动 CRC32。辅助 64DE80 及内联代码都是：

```text
H = 0
对每个有序输入 v：
    H = uint32(ROL32(H,1) + v)
```

位置值 `P=trunc(X/10)+(trunc(Y/10)<<16)`；8 位朝向取整为 `(((raw>>7)+1)>>1)&255`。输入顺序：

1. Infantry 数组：每个对象的 P + 主朝向。
2. Unit 数组：P + 主朝向 + 副朝向。
3. Building 数组：P + 主朝向。
4. House 数组：每个 House+241 的 MapIsClear。
5. 五个显示层列表依次：P + RTTI；Anim 的特殊本地 ID=-2 排除。
6. Logic 列表：P + RTTI，带同样的本地动画排除。
7. Scenario+218 的原版随机数发生器调用 65C780 的一次输出；该调用也推进发生器的两个索引。

顺序、数量影响结果。飞机参与显示层/Logic 列表，不能因为没有专用 Aircraft 数组就认为它不校验。Z 在该函数中被读到临时位置但未混入结果。生命、钱、当前任务、目标指针、TechLevel、侧栏图标、DLL 代码不是该函数的逐项直接输入。

最近 256 帧存储位于 `B04474`。DoList 消费的 FrameInfo=1C 分支 64C65F..64C6F2 读取 Event+3 Frame、Event+7 CRC、Event+0D Delay，以 `(Frame-Delay)&255` 选择历史槽；不一致跳 64CC61 错误路径。受控测试验证匹配与不匹配分支。完整发送间隔及不同平台/MOD 的额外校验不在本次确认范围。

本次原版完整摘要测试验证：单独改 HP、钱、任务、已关联类型的 TechLevel 或 Z，当前摘要不变；改跨量化界限的 X、朝向、MapIsClear、随机数状态、对象顺序或数量，摘要改变。**没有执行后续世界 tick，这不是锁血/无限钱联机认证。**钱不同会导致生产停顿/单位出现时序不同；生命不同会导致死亡/对象数量不同；这些后果已能被本函数检出，无需假设另有直接生命/金钱校验。

## 飞机到达推进不是一次新的网络下令

6385C0 `TryNextPlanningTokenNode` 先 QueueMission(Wait=28,start=true)，再把规划节点事件 +1D 设为 2，放入 `AC4B48` 内部列表。列表由 `637550 → 652230` 消费；652230 把事件插入本机 DoList，通常调度到下一帧，**没有重新走 OutList**。本次受控执行验证在 Session=3 仍是 DoList+1、OutList+0。

原版一致性依赖各端相同规划状态和相同推进。现有补丁对本机所属飞机增加到达回调，不能称为“只放宽事件生成”。兼容模式则是本地判断到点后发送新的正常 Move，对方只需一致消费新的命令。

1.0.39-pre.1 在原有路径点下拉框新增：

| WaypointMode | UI | 联机行为 |
| --- | --- | --- |
| 0 | 关闭 | 关闭 |
| 1 | 兼容路径点 | 本地规划，发送正常 Move |
| 2 | 原生优先 | 单机原生，联机兼容，保持旧行为 |
| 3 | 原生下单（联机实验） | 只启用 7E2908 的原生入口判定；416AB6 到达补丁恢复 |
| 4 | 完整原生（联机实验） | 入口及本机到达推进都启用，可能不同步 |

原生实验需要路径点按住键留空以跟随游戏按键；自定义键继续走兼容模式。Mode3 可能只执行首段/不能续飞，不能承诺只放开入口就有完整原生飞机规划。Mode4 作为用户明确要求的实验保留，不宣称兼容联机。统一管理器仍是唯一补丁所有者，切换模式/关闭按组恢复。Shared ABI 升至 73，EXE 与 DLL 必须配套。

## Sticky 的新线索与排除

普通 Unit::ReadyToNextMission=744270 在 current+AC==6 直接 false。原版 QueueMission=5B35E0 的 start=true 也会先问 readiness，不能直接绕过。16 种组合覆盖 Attack/Move/Guard/Enter/AreaGuard/Stop/Wait/AttackMove 与 start=0/1，均保留 Sticky，只改变 queued+B4。

Unit::AssignDestination=741970 的前缀在 **Type+D6A BalloonHover=true**、新目的地为空、旧目的地与当前攻击目标匹配等状态下，确实经 741A6D 调 ForceMission(Attack)。相同受控状态下 BalloonHover=false 不走该分支。这说明“接收后继可以触发强制任务”的研究方向有依据，但这条线索不能通用于普通战车/磁电，不通过改本机 BalloonHover 来冒充同步解除。完整接收事件组合、其他类型特定分支和真实游戏效果仍未确认。尚未找到通用解除，不代表已证明不存在。

## Ctrl+Alt 建筑格连发

原版 Infantry::GetActionOnCell=51F800 经 Foot/Techno 判定；Ctrl+Alt 的一条正常格子路径返回 GuardArea Action=26，而非 Enter。原版 Foot::Active_Click_With=4D7D50 把该 Action 映射为 **AreaGuard Mission=11、target=格子、destination=null**。另一个格子查询分支返回 PatrolWaypoint Action=51，不能把所有点击场景视为同一事件。

本次对该原版点击派发重复 32 次，参数保持相同。对 Infantry::CanEnterCell 的 51C2E1..51C377 原版任务门槛重复 32 次：普通步兵在 AreaGuard=11 下仍被拒绝；Engineer=true 对照可通过这一处门槛。它不是“普通步兵 Ctrl+Alt 连点已经成功进入”的证据，也不排除已有规划/驻军建筑/MOD 等不同场景。故没有把这个受控失败候选默认替换掉现有的规划进入功能。现有功能继续用原版任务/规划事件，不恢复 C4 能力修改。

## 测试与未决

- `assist/tests/network_state_capture.py`：47 个原版捕获指令用例，包括完整帧摘要与独立公式对照、FrameInfo 比较、内部规划到 DoList、Sticky 排队、BalloonHover 条件分支、Ctrl+Alt 分派及重复进入门槛。
- `assist/tests/air_network_runtime.py`：24 个编译后 Runtime 用例，覆盖会话、两种联机实验、入口返回、补丁切换/关闭恢复，不把安装成功称为飞行成功。
- `assist/tests/air_waypoint_host_test.py`：主机记录、按键、取消、到点继续以及实验模式输入所有权；自定义键回退。
- `assist/tests/sticky_guard_runtime.py`：34 个 Sticky 现有行为回归。
- `assist/tests/infantry_entry_runtime.py`：65 个现有进入事件/部分原版门槛回归。
- Win32 EXE/DLL 使用 Zig 0.13.0 构建。原有工具链/弃用警告不等于本次修改引入的错误。

未决：真实双机测试；原生下单实验能否在实际飞机上完整续飞；原生全模式不同步触发条件；Sticky 通用可逆事件组合；Ctrl+Alt 在已有规划等不同状态中的完整寻路/到达效果；平台额外校验与发送周期。


## Sleep、Harmless 与火力分配

本次额外核对 MissionControl 数组 A8E3A8（每项 20h）、原版任务 AI 5B3060、Sleep 处理器 5B2E10、Harmless 处理器 5B2E20、QueueMission/Unit readiness。当前捕获的实际配置如下；MOD 可以改变这些规则，不能只看任务名字。

| 属性 | Sleep=0 | Harmless=23 | Sticky=6 对照 |
| --- | --- | --- | --- |
| Zombie（抑制一类原版自主任务切换） | 是 | 否 | 否 |
| Retaliate（原版反击许可） | 否 | 否 | 是 |
| Scatter（自动散开许可） | 否 | 是 | 否 |
| NoThreat（候选目标的威胁筛选标志） | 否 | 是 | 否 |
| Recruitable（AI 招募许可） | 否 | 否 | 否 |
| Paralyzed（任务控制标志，非无敌） | 否 | 否 | 是 |
| 普通空闲战车接收新 Attack/Move/Guard 后退出 | 本次受控测试可退出 | 本次受控测试可退出 | readiness 直接拒绝 |

两个处理器都只返回 450；原版任务 AI 把它写入 UpdateTimer，当前任务不改变。把帧推进到 10000 再执行该 AI，仍保持对应任务。因此 **450 是下一次任务处理的间隔，不是450帧后自动醒来**。正常退出依靠后来收到的新任务经过 readiness 和 NextMission；特殊运动、装载、受控、MOD等状态可以增加条件，不能承诺所有单位立即切换。

Retaliate=no 只限制受击反击，不能单独推出不会主动寻敌。Sleep 用于留枪时，目标入射程仍需分配器明确发送 Attack；不会仅因目标进入射程而按450帧计时自动醒来。

NoThreat 并非“不怕攻击”。CanAutoTargetObject=6F7CA0 在 6F7DFF..6F7E0B 检查候选目标自己的 MissionControl+4，为真会跳过该候选；手动攻击、范围伤害、已发射弹体不因此消失。因此 Harmless 不仅影响己方开火，还可能改变敌方原版自动选敌，且允许散开，不宜直接当作精确站位留枪。

Sleep 更适合作为可解除的短时留枪候选：通常抑制自主战斗/反击/自动散开，目标真正进入原生射程后发 Attack，取消或无合适目标时明确发 Guard。不要仅停止分配器扫描，也不要把键盘 S 的 Idle 事件等同于明确 Guard；必须确认恢复命令已提交/被观察，尊重玩家手动任务。它不提高伤害、不清武器冷却、不追回已出膛弹体，也不保证接收命令前已经停止开火。

当前 missile_bridge.h 已部分这样实现：防御建筑等待尚未入射程的分配目标时使用 Sleep；移动单位使用 Attack 接敌，不发送 Sleep 或 Sticky。关闭功能时使用 Guard 释放自己控制的任务。把移动单位也改为留枪会带来不接近目标、漏拦截的取舍，应作为独立策略研究，设最大保留时长、紧急目标唤醒、手动优先和失效恢复。

本次新增 8 个原版受控用例：Sleep/Harmless 各两次任务 AI 调度仍保持任务，外加各自转 Attack/Move/Guard 的 6 次原版排队/切换，均通过。未验证完整实机受击、寻敌、散开和所有 MOD 行为。

## 后续 Sticky 候选（假说，不作为已验证功能）

| 方向 | 为什么值得追 | 边界 |
| --- | --- | --- |
| 强制散开/原版停止的深层分支 | Scatter/Idle 已是同步事件，可继续追下层任务与运动控制是否有绕开 readiness 的分支 | 普通停止或排队本身未证实有效，不能把多发几次称为解除 |
| 载具、坦克碉堡的进入/离开 | 原版 Radio/装载/释放可能强制重置任务，若由同步事件触发，各端可一致走入 | 必须先证明 Sticky 能进入这条流程；有载具、容量、地形条件 |
| 临时任务覆盖再恢复 | 研究原版 OverrideMission/Mission_Revert 是否能通过同步事件触达，并改变恢复后的任务 | 可能恢复回 Sticky；禁止直接只在本机调用来冒充同步 |
| 部署变形/对象替换 | 同步部署可销毁旧对象、创建新对象，天然重建任务状态 | 仅可部署型号；可能换身份/状态，不是同一辆普通战车的通用解除 |
| 可恢复的替代任务 | 新需求可考虑 Sleep/Harmless/Ambush 等而非 Sticky=6；已有受控任务切换证据 | 是替代设计，不会自动解救已经 Sticky 的单位；实机停火/寻敌行为另测 |

研究优先级：先追同步 Scatter/Idle 及 Radio 入口，再查临时覆盖。原有类型已满足 BalloonHover 的条件路径可以单独验证，但单端修改 BalloonHover 会令接收端模拟分叉，不能作为通用联机修复。

