# v1.0.36：路径点、坦克碉堡、磁电落地与载员显示

调查前已检索 INDEX、pre.9/pre.10/pre.12 笔记及持久 SQLite；只记录本轮新增结论。

## 样本与证据范围

- 兰博玩样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- 战网样本 SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`
- 地址均为这两份 PE32 样本的原始 VA。反汇编和受控 x86 执行不等于实机寻路、图形表现或网络同步验证。私有 EXE 不发布。
- 测试：`assist/tests/{infantry_entry_runtime,infantry_waypoint_runtime,air_waypoint_runtime,c4_arrival_runtime,tank_bunker_runtime,magnetron_drop_runtime}.py`；纯逻辑：`review136.cpp`、`magnetron.cpp`、`magnetron_rof.cpp`、`air_waypoint_host_test.py`；报告在 `verification/review-136/`。

## 飞机地面路径点

问题：点击点偏移，飞机经过第一点便返回机场。

`DisplayClass::ProcessClickCoords` 0x692300 是 thiscall，接收战术视口局部像素及多个输出指针。函数内部加上视口原点 0x886FA0/0x886FA4。原生调用点 0x4AE546–0x4AE553 先减原点，0x4AE571 调用；0x4AAC92/0x4FB42A 附近同样处理。此前直接传全客户区像素造成原点重复。

`TacticalClass::ClientToCoords` 0x6D2280 输出 Z 为零（0x6D2321、0x6D232C），不能直接作为坡地标记高度；调用 `MapClass::GetCellFloorHeight` 0x578080，thiscall(map=0x87F7E8, CoordStruct*)，返回 int 地面高度。现在只减一次原点，标记使用地面 Z。

原生飞机到达分支 0x416AB6 可切换任务状态；兼容路线不应把 AI 的暂时攻击/返航当作用户取消。现在保留队列，线段穿越也识别到达，提前返航重发当前点；用户 S/D/X 或新的普通鼠标命令仍取消。最终航迹仍需实机确认。

## 步兵原生路径点与 PlanConnect

恢复追加原版 MegaMission 路径点 + PlanCommit(43)。普通步兵 Enter(7)、工程师 Capture(8)、间谍 Enter(7)、原生 C4 Sabotage(17)，目的地为建筑对象。移除本地能力及归属预检，不再安装 0x51C32B/0x51A4D8 两个 C4 能力覆盖；仍验证来源身份、存活与玩家归属。游戏接收端保留原判定，所以普通步兵可能仍被游戏拒绝，也不会凭空拥有 C4。

另调查 PlanConnect(42)：0x637E00 中的 0x637E9F 分支，6E6F20/6E6E20 解析对象，遍历 AC4B30/AC4B40；操作单位规划 token +0x514，通过 705D20/705D10、635F20、639800/6367B0 合并路线。该路径不是已证实的能力绕过，因此没有作为成功保证或另一路强制执行入口。

## 坦克碉堡

原版 `UnitClass::MouseOverObject` 0x73FD50，在 0x74021E–0x74027A 对己方空 TankBunker（BuildingType +0x16AB）检查 QueryRadio(15)=1、没有现有联系（65AE30）、乘员链 +0x118 为空，返回 **Action::Move(1)**。旧代码只接受 Enter(3)/Repair(11)，因此拦掉合法碉堡动作。

原版 `UnitClass::SetDestination` 的 0x742CAF–0x742D24 对碉堡建立 Radio(2/14) 联系后调用 Foot 设置目的地 0x4D94B0。修复：仅碉堡接受 Move，提交 Mission::Move(2)，destination=建筑 ID/tag 0x34。分配器继续按一车一堡预留，+0x2E4 双向 BunkerLinkedItem 用于完成观察。受控测试模拟 MouseOverObject 返回值与完成状态，不声称已运行完整进驻运动。

## 磁电释放和致死落地

`TechnoClass` 释放函数 0x70FEE0，thiscall(bool)，ret 4：Source +0x2AC 指向被吸单位，Victim +0x2B0 指回磁电。释放清除双向联系；原生高度虚函数 +0x1C8 >0 时，置 Victim +0x425/+0x427，+0x428=来源，+0x42C=来源所属方，调用 Crash 虚函数 +0x3A0（Foot 实现 0x4D5660），清目的地。

Jumpjet 落地函数 0x54CA90：0x54CC36 在落地/过桥相关条件及 +0x427 状态下处理碰撞；0x54CC91 获取 Cell（5657A0），遍历 Cell +0xE4 / Object +0x30。0x54CCAE 附近 WhatAmI 为 Aircraft(1) 或 Building(6) 进入 0x54CFB7 致死分支。Terrain(15) 会先破坏地形物，再继续落地；**有树/地形对象不等于必定摔死**。0x54CE26 调用目标的 CanEnterCell 虚函数 +0x1AC，非正常进入且非水面分支可致死。

新自动摔落读取当前位置地面格：验证双向磁电联系、敌方地面载具、离地、非桥面；检测建筑对象或 Rock land(7)，再通过原版 Idle(0x06) 对磁电下达停止。Rock 是原版地形的保守代理，未对所有 MOD/移动类型证明必死。没有调用寻路/radio/RNG 做本地决策，没有直接执行 Crash 或清目标内存。事件接收前可能移动，因此不能保证每次停止都命中障碍。

独立开关与独立类型清单；单独开启也刷新世界索引。停止入队后等待磁电联系解除，30 帧仍未解除且仍符合障碍条件才重试；联系解除后恢复轮换，人工下令 token 变化清理等待。原有轮换不再把开火后仍正计时/暂停的 rearm 误当成不能换目标；单个候选允许重新尝试。

## 载员与显示

原生 hover 标志 Techno +0x431：0x4AAEEC 置位，0x6F9E65 在单位更新中清理；选中 +0x83。载员 count/head 为 +0x114/+0x118，NextObject +0x30，逐个验证 Transporter +0x11C，计数上限 256，拒绝循环/断裂列表。

工程师 +0xEC3、潜入 +0xEBE、C4 +0xEC2、Ivan +0xEAE 为 InfantryType 能力。爆破武器 Suicide +0x144 依据 YRpp WeaponTypeClass 定义（Drain +0x142、FireInTransport +0x143）；这是头文件布局依据，尚未单独反汇编该字段的 INI 写入。

载员显示完全只读，按类型合计，hover/选中与危险乘员常驻分开开关，采样 20 Hz；危险持久显示不要求敌方控制。UI 五大分类不会新增运行线程或注入通道。小地图只对已知颜色插值，中心格黑幕不借邻格颜色填充。队列空闲/暂停分别红/黄，正常进度和完成待放置不闪。
