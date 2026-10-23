# pre.8：FA2 资源参考、显式 Sticky 与独立全局摔落

调查前检索 INDEX、pre.4/pre.6/pre.7 笔记和持久 SQLite v25。GitHub 基准 `c4351b11c975d13b8c5f73761168e4fe87aeb193`。不提交私有 EXE、FA2 源码副本或用户原始日志。

## 证据与范围

- LBW EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网 EXE SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 用户本轮确认地形已能读取；仍报告 SHP 单位和覆盖物缺失。没有收到能定位具体失败步兵的新日志或 MOD 资源，因此本轮不能宣布全部 SHP 已实机修复。
- 受控测试执行编译后的 DLL、原版任务函数和事件构造器；Win32、世界、事件最终交付与地形为夹具。双模式事件队列验证不代表双机同步，摔落准入验证不代表完整下落物理。

## FA2 参考与共用渲染

一手参考：[EA CNC_TS_and_RA2_Mission_Editor](https://github.com/electronicarts/CNC_TS_and_RA2_Mission_Editor)，`MissionEditor/Loading.cpp` 中 `FindUnitShp`、`LoadUnitGraphic`、`LoadOverlayGraphic`，以及 `IsoView.cpp`。独立实现资源规则，没有复制其渲染器源码。另参考 [OpenRA SHP loader](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Common/SpriteLoaders/ShpTSLoader.cs) 与 [YRpp ObjectTypeClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ObjectTypeClass.h)。

FA2 对 Theater 扩展名、NewTheater 第二字符、单位/地形色盘及矿石色盘分支处理。原观察窗统一给覆盖物套 ISO 色盘，并且只在地形色盘成功后才开始覆盖物加载。现在从游戏目录及已有 MIX 优先级解析 ARTMD/RULESMD（缺失回退 ART/RULES），加载真实图像名、剧场文件、Palette、TerrainPalette 与矿石标志；覆盖物独立选择色盘。自定义单位色盘保留阵营映射；优先已加载的类型专用色盘，不能用默认单位色盘覆盖自定义色盘。全零未初始化 live ColorScheme 拒绝作为有效色盘。

剧场表 `0x7E1B78`，步长112；字段 +48 控制名、+68 ISO 色盘、+78 扩展名、+86 NewTheater 字符。原 ImageFile +1F8 修复继续保留。此实现未宣称覆盖 FA2 全部扩展插件、所有跨剧场回退、动态地图 INI 修改和 MOD 自定义脚本。

步兵动作仍读原生序列表 type+E3C（42条，每条36字节）、object+6C4 当前序列、+F8 动作阶段；使用 `0x7EAEFC` 方向 LUT。原版 `Infantry::ShapeNumber 0x518D80` 证据见 pre.4/pre.6。地图编辑器的静态预览不代替运行时动作状态。

观察窗原来每帧先清除不等于当前 ImageKey 的图像。连续动画或转向且渲染预算耗尽时，上一张已经成功绘制的图像也丢失。现在以对象指针和 ID 保留上一张真实姿态，直到新姿态完成；离开窗口或换局清理，不显示示意占位。

用户补充队形也要同样绘制后，移除队形独立的 memory-first 捕获分支，统一调用 `captureActor/renderActor`。队形按预定朝向选 Ready 帧，VXL 身体/炮塔预览朝向一致，保留预乘半透明像素；源 SHP/VXL 不随每次拖拽转向重新解码，方向图片单独缓存。缓存键包含类型、阵营、实际炮塔变体；不再限制为64种模型，但仍有总面数和像素预算。

`game_assets_host_test.py` 从嵌套 MIX 读取未选中且无内存 SHP 的步兵，使用真实方向 LUT、各帧不同颜色、Ready 与两帧 Walk 序列，验证像素而非只验证解码返回值；另测 NewTheater 墙、矿石色盘、全零 live scheme。`watch_host.cpp` 验证300种单位同时转向、预算不足时保留真实图像；`formation_host.cpp` 验证共用接口、方向缓存、透明度、所有权变更和指针淘汰保护。均不是 Windows 截图验证。

## 显式 Sticky：为何只能先做单机

pre.7 已移除自动规划意外发送 Sticky=6；本轮用户要求显式快捷键，不能把该锁死状态再次自动套给普通任务。

原版 `QueueMission 0x5B35E0(mission,start)` 是 thiscall/ret8；Unit readiness `0x744270` 对当前 +AC=6 返回 false。MegaMission 与 MegaMissionF 都进入 `0x4C71CA`，F 不是强制解除参数。Idle 接收器 `0x4C74CB` 不对普通单位通用强制 Guard；Scatter `0x4C7859` 到 `Unit::Scatter 0x743A50` 仍主要 QueueMove/SetDestination，也不能可靠解锁。排除“换成另一种普通网络事件就能通用解除”的假设。

`ForceMission 0x5B2FD0(int mission)` thiscall/ret4，执行原版任务/队列/计时器重置；`NextMission 0x5B3570()` thiscall/noargs 提升 +B4 到 +AC。显式驻守在单机模式0/5、统一入口有效且前缀机器码吻合时调用原版函数；没有改引擎代码。模式3/4返回 -40，不作本地任务修改。网络同步解除路径仍未实现。

记录 pointer/id/house/match 的所有权，只自动解除本功能登记的单位：新原生命令已排队时调用 NextMission 保留该命令；普通手动下令、再次按键、取消自动任务和工具退出可解除。右键取消/拖屏本身不解除；对象消失或ID复用只清登记，不写新对象。关闭其他自动功能后，驻守维护仍继续运行。防空、躲避和自动规划不得抢走驻守单位；显式全局摔落仍允许释放它正在吸起的危险目标。

默认 Ctrl+Alt+S，可修改及双击绑定；金色标记。Sticky 指令附加在命令枚举尾部，无 Shared 布局改变，仍升 Assist 协议到66避免旧 EXE/DLL 混配。此开关没有宣称保存游戏/重新启动工具后还能恢复驻守登记。

## 独立全局摔落与水陆判断

移除手动攻击暂停和 source.target==victim 对自动摔落的否决；自动索敌仍尊重手动控制。开关独立遍历己方活动磁电，要求 source+2AC 与 victim+2B0 双向关系、敌方活着的地面载具、最新格子与下方内容。规划器不再要求当前选中武器的射程快照有效，活动吸取关系和 DropHazard 才是本次摔落依据。保留独立单位类型清单、重复命令抑制和执行前复核。

原版 LandType：Water=2、Rock=3、Beach=6、Rough=7。**更正 pre.136 的旧判断：7 不是岩石，不能因粗糙地面而一律摔。** SpeedType：Foot0、Track1、Wheel2、Hover3、Winged4、Float5、Amphibious6、FloatBeach7。MovementZone 的两栖族为3/4/5。

`Unit::CanEnterCell 0x73F0A0` 的 `0x73FA9E..0x73FADC` 读取 cell+EC、type+67C，查 `0x89EA40 + 4*(LandType*9+SpeedType)`；零速率表示不可通行。悬崖依据落点不可通行判断，不把邻格高度差当成可摔。原 Jumpjet 落地 `0x54CA90` 在 `0x54CDF6` 判断 Water2、Beach6，`0x54CE26` 调单位虚表 +1AC CanEnterCell，再进入相应死亡/水面处理。

**MovementZone 实际是 type+5B4**：原 INI 字符串位于 `0x8431C8`；`0x71605E` 取默认值，`0x716065` 引用键，`0x716079` 调 `0x474E40` 解析枚举，`0x716081` 写 +5B4。开发中 +5DC 猜测被反汇编否定，未随预发布交付；不要沿用该猜测。

普通陆车落水、纯船落陆可摔；FloatBeach 允许海滩；悬浮/两栖不因普通水陆本身而摔。建筑、树、矿柱、地面载具仍独立构成机会。下方载具需落在同一格且贴地，排除被运输、吸起、下落、Jumpjet、空中层及低空悬浮。桥面维持保守排除。这里只判定摔落机会，不保证树会杀死坦克，也不承诺零延迟预判漂移。

验证见 `verification/review-137-pre8/`。需要实机继续确认 MOD 的具体 SHP、队形画面、磁电实际下落物理及游戏保存/载入行为。
