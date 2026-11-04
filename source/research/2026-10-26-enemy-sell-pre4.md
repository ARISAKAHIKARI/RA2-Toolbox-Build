# 对手建筑变卖实验与所属方自毁事件（1.0.39-pre.4）

## 问题与证据边界

用户在厘清 MegaMission 与 Sell 路径后要求实现变卖对手建筑。本轮先检索 INDEX、持久 notebook 的 direct-orders-1、player-132-trainer-samples 及 pre.3 网络研究；原项目曾实现跨玩家手动事件，后按用户要求单独禁用 Sell，当前请求恢复此项。

支持游戏样本 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。本轮不新增发送器或接收器语义假设；复用已核对的原版对象事件构造器及 Sell 所属方检查。合成世界中的最终 DLL、原版构造器和部分原版接收分支能验证准入/序列化/接收所属方门槛，不能证明实际平台网络传播、完整 DoList 调度、建筑拆除或退款。

## 实现

新增恢复的 UI 开关“变卖对手建筑（实验性）”，默认关闭，控件722。共用“出售被吸取目标”的按住键和鼠标键；默认 Ctrl＋鼠标侧键2，可按原配置修改，不要求同时开启出售己方被吸取目标。

保留已有 `SpecialClick/SellEnemyBuildings` 设置，但读取时还要求 `Experimental/EnemyBuildingSellOptInVersion>=1`。旧版本历史配置可能保存true，缺少本轮独立标记时不自动重新启用。保存当前选项时写入标记及当前开关；与旧 PlayerCommandOptionsVersion 的自动迁移分离。

统一 `enemyBuildingSellAllowed` 在指针候选、对象事件网关及最终构造授权复核：世界中存在、存活、建筑、当前阵营的实际敌人、有效可玩所属方。盟友、己方、步兵/战车、被动/中立/观战/已失败阵营及失效对象不因这一开关放行。保留聊天、对局、新鲜度、手动版本、队列容量和预算等现有检查。

发送链：手动输入 → `nativeObjectEvent(建筑Owner索引, Sell=0x16, 建筑网络ID, SellPlayerTest)` → `authorizePlayerOrder` → 原版对象事件构造器 `4C65E0` → 统一队列。联机写 OutList，单机写 DoList并保留已授权的事件Owner。即使用户选择 ClickedEvent 兼容后端，跨所属方请求仍走构造器后端，避免 ClickedEvent 使用当前本机所属方替换目标所属方。每个请求只入队一次；“已入队”不表示变卖完成。

不改建筑Owner，不切换CurrentPlayer，不改原版Sell所属方检查，不添加模拟层补丁。`SwitchAvailable=false`仍保持。既有己方战车/被吸取目标出售入口保持各自条件。

## 原版接收路径与限制

Sell事件类型为0x16，不是MegaMission，也不是Selling任务19。原版执行器 `4C6CB0 → 4C6F12` 解析对象，`4C6F3B..4C6F48` 比较事件的有符号House字段 `+2` 与对象Owner `+21C → House+30`，不相等退出；满足条件后在 `4C6FA2` 调用虚表 `+1A0`，参数-1，建筑方法为 `447110`。

测试接收终点记录Sell调用，未运行447110完整拆除/退款。所属方字段匹配只证明能通过这一道接收门槛；平台可能在更早的节点映射、传输或调度阶段处理这些事件，目前没有实机双机验证。尤其原版DoList存在对House +1EC/+1ED的遍历条件，不能由“单机队列成功追加”推出对AI建筑一定执行。

排除的说法：成功入OutList就必然在对端执行；将玩家编号写成负数可变成观战权限；移除本机Sell接收检查就能让远端也接受；MegaMission Selling=19等于完整Sell。当前功能是可复测的事件发送实验，实际平台效果需实机验证。

## 验证

`assist/tests/enemy_sell_runtime.py` 覆盖四类会话的开关、事件所属方/引用、输入路由、无效对象/所属方、聊天/过期、队列满后重试与重复请求；以及最终DLL构造事件的原版接收所属方正/负例。最终结果与二进制指纹记录在 `verification/review-139-pre4-enemy-sell/`。

最终 DLL SHA-256：`fe3a45c75897af363776b6ef430b1b58cfc62a41cab42115930b74f3252d7619`。最终95项全部通过（84项队列/准入、8项部分原版接收分支、3项原版House调度门槛），既有己方右键变卖4项回归通过。Host/Runtime完整构建和最终PE32/版本/依赖/ABI检查通过；版本1.0.39-pre.4，IPC保持76、Shared保持4165672字节。

## 联机“观战自爆”与指定所属方 Destruct

新增问题：点击观战后自爆是否使用原版事件，能否在事件中指定其他所属方？仍使用本笔记支持样本；调查前检索 INDEX、direct-orders-1 与持久 notebook。原版枚举没有独立名为 Resign 的事件，不能把退出/移除玩家/自毁混成同一种。公开名称交叉核对 YRpp GeneralDefinitions.h、HouseClass.h、NetworkEvents.h（https://github.com/Phobos-developers/YRpp）。

- `Destruct=0x08` 是不带额外负载的事件，原版构造器 `4C66C0` 为 thiscall(player,eventKind)，ECX指向事件。接收器 `4C6CB0` 以事件头有符号HouseIndex（+2）查House数组；Destruct分支 `4C7BDF` 以该House为ECX调用 `4FC980`，无栈参数。
- `4FC980` 对 House+1F7(IsWinner)、+1F6(IsGameOver)、+1F8(IsLoser) 已成立的状态不重复设置。正常状态下设 +1F6=1，并将 +298 定时器起点设为当前帧、+2A0时长设0。该函数并未当场遍历销毁对象。
- House AI `4F87C2..4F87FA` 的静态控制流检测上述标志/计时器，到期后清+1F6并调用 `House::DestroyAll=4FC6D0`；后者遍历匹配归属对象并进入原版伤害流程。完整House AI、伤害/销毁、胜负结算和转观战本轮没有动态执行。
- `Exit=0x13 → 4C7903` 设置全局A83D48=1，本分支不调用Destruct；`AboutToExit=0x27 → 4C7CF5` 是退出协调，部分路径产生Exit；`RemovePlayer=0x23 → 4C7B73` 从payload+7另取House，具有模式/人类控制分支，部分路径调用4FC980，不能直接当成相同的包。
- 原版UI静态线索：48CA34、48CAB2、48CBAE以当前House+30和事件8调用4C66C0，随后追加OutList；有的分支还调用退出流程。尚未确认用户平台特定“观战”按钮完整点击链，不能把它直接归到某一个地址。647040产生的是Options=0x0C，不是Destruct。

`assist/tests/destruct_receiver_probe.py` 的20项受控原版执行全部通过：16项覆盖事件House0/1、CurrentPlayer0/1、合成Observer指针及四种House初始状态；两项Exit确认不调用4FC980；两个负玩家编号构造例得到Empty/FF，不送执行器。测试实际运行构造器、事件接收器和完整4FC980标志/定时器函数，未运行完整DestroyAll。结果见同验证目录的destruct-receiver-native.json。

结论：**就已执行的接收分支而言，合法的事件House字段能改变被标记结束的所属方，且没有比较它必须等于本机CurrentPlayer/Observer。**这支持“其他House的Destruct能被原版执行器理解”，不证明事件头与真实发送节点不一致时平台会转发、纳入DoList并在所有端执行。真实观战按钮链及联网传播仍待验证。本版只加入对手建筑变卖实验，没有新增针对整个所属方的自毁按钮或绕过平台网络处理。


## 后续更正（pre.5）

[自毁按钮与发送器研究](2026-10-26-destruct-button-pre5.md)已确认：支持样本原版联网发送器在protocol0/1/2均把OutList事件House覆盖为CurrentHouse，再复制本机DoList/序列化。因此本篇foreignHouse构造、入队与直接接收器测试仍成立，但不足以实现原版联网跨所属方变卖；改写后会碰到Sell的Owner比较而被拒绝。Destruct则会误作用于本方，pre.5已专门阻止联机其他所属方的自毁提交。
