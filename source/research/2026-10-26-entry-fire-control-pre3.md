# 建筑格进入、火控休眠预留与低伤害索敌过滤

## 样本和问题

基线为实验分支提交 `763f9939112fbe4a0d3043e8d3ba6d3ea3f4284e`。支援 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。调查前已检索 INDEX、pre.9/pre.10/pre.12、火控预留和 2026-10-26 任务研究，以及持久 SQLite v58。

用户要求：普通步兵强制进入提供 Capture=8 / Sabotage=17 两种建筑格实现；调查工程师 Ctrl+Alt 点击边缘建筑或建筑上叠车的事件；防空单位提前 Sleep 等待分配目标、射后继续留枪；增加不选伤害倍率不超过 2% 目标的全局开关。

依据为源代码审计、捕获中有效指令的受控 x86 执行和最终 Runtime 合成世界测试。没有本轮 Windows 实机寻路、到达、占领、爆破或真实双机联机通过的证据。私有 EXE、内存转储和原始反汇编不提交。

## 建筑对象和建筑格是不同的事件参数

`Infantry::MouseOverObject=0x51E3B0`，虚表 `+74`；`ObjectClickedAction=0x51F190`，虚表 `+144`。原版默认修饰键数组 `A8EBF8..A8EC0C` 为 Alt/Alt、Ctrl/Ctrl、Shift/Shift。受控可占领敌建筑、工程师和正常对象查询中，无修饰、Ctrl、Alt、Ctrl+Alt 均分类为 Action::Capture=9。对象点击再经 `Foot::ObjectClickedAction=0x4D74E0` 的 `4D7716..4D773A`，调用 ClickedMission：

```text
Mission=8, Target=null, Destination=建筑对象, Nearest=null
```

原版 `ClickedMission=0x6FFBE0 → QueueMegaMission=0x646E90 → 构造器0x4C6860 → OutList` 的受控执行得到上述包。网络身份打包及地图/键盘查询使用替身，不是录到的实机鼠标流。

另一条原版格子点击路径 `Foot::CellClickedAction=0x4D7D50` 确实有：

| 上游已选动作 | 原版分支 | MegaMission 任务 | Target | Destination |
|---|---|---:|---|---|
| Capture=9 | `4D8110..4D8138` | 8 | 空 | CellClass |
| Sabotage=16 | `4D8182..4D8198` | 17 | 空 | CellClass |

对这两处的测试**在动作选择边界注入了动作编号**，之后执行原版分派、ClickedMission、发送器和构造器；这证明包格式和分支存在，不证明边缘/叠车一定会选中它们。正常非地图扩展的 Ctrl+Alt 格子夹具，工程师和普通步兵均分类为 GuardArea=26，发 Mission=11、Target=格子、Destination=空。

因此，“边缘/叠车使原本建筑目的地变成格子”是合理待验假设，尚未捕获其完整屏幕命中与动作选择链。不能把建筑对象正常点击得到 Capture，或强制动作分派得到 Capture 格子包，说成已复现该实机触发条件。

## 普通步兵能否走进建筑格

原版 `Infantry::CanEnterCell` 的 `51C2E1..51C377`：任务 8 直接进入后续目的地判断；任务17还读取 InfantryType `+EC2` 的 C4；任务11则检查 `+EC3` Engineer。后续 `51C37D..51C3BC` 既比较占用建筑是否等于单位 `+5A4`，也调用 Map::GetCellAt(建筑坐标) 并比较得到的格子是否等于 `+5A4`。

新增测试将原版任务门槛与这些目的地比较连起来执行：普通步兵 + Capture=8 + 目的建筑所在格可到达 `51C71B` 的放行分支；相邻其他格不通过。普通步兵 Sabotage=17 被更早的 C4 判断挡住，原生 C4 单位则可继续。同一原版 Capture/Sabotage 任务 AI `4D4B20` 对己方人类控制对象保留显式格子目的地。以上只是局部门槛和任务 AI，仍不等同完整寻路/到达成功。

实现提供两选项，默认 Capture：一条原版 MegaMission，Target 空、Destination 是被点击建筑占地格的 `Cell ID + 0B`、Nearest 空。对工程师/间谍/普通步兵不再自动改选另一任务。普通点击直接提交，按住原生路径点键时只追加节点，由原生松键提交。移除普通点击强制绕行 PlanningAdd/PlanCommit 的旧流程。

建筑被坦克覆盖时，沿点击格 `Cell+E4 → Object+30` 链解析建筑，仍用被点击的格子。原版屏幕命中查询没有被修改。输入新位 `EntrySabotage=262144` 随一次 Intent 固定，模式切换不会把一个批次的剩余单位改成另一种任务。保留统一线程、构造、所属方、对局、有效期和队列路由，不安装 C4 能力补丁；C4 输入取消旧任务时也不调用本地 Sticky ForceMission 解除路径。

## 防空提前留枪与射后预留

增加“提前休眠留枪”，默认开启（防空火控总开关默认仍关闭）。原智能/平均分配作为低位模式，ReserveSleep=2 为策略位，内部和外部计算使用同一纯算法。

- 建筑、战车、步兵对尚未进入原版 IsCloseEnough 的分配目标提交 Sleep=0；冷却尚未结束和足额覆盖后的多余射手也 Sleep。射击目标字段为空，分配的目标 ID 仅保存在工具记录里。
- 目标进入原生射程且冷却接近完成时提交 Attack=1。当前覆盖攻击先于预留命令，避免队列预算先花在休眠而漏掉必要拦截。
- 观察到本工具已接管攻击的原生 rearm 起点改变、当前攻击目标匹配且仍在射程，暂记一次预计单发伤害，最多12模拟帧。目标 HP 变化、消失或过期即清除，避免把已观察到开火的单位马上当成“未提供任何覆盖”。这不是弹体命中证明，也不是完整在途伤害账本。
- 原生 BurstFiring 期间以 TransientBusy=-2 跳过自动重新发令；提交 Sleep/Attack 时仍检查当前可用性，自动释放 Guard 也在提交前重新检查连发状态，防止外部计算返回前才开始的连发被旧结果打断。结束后再规划。显式关闭主功能或退出则允许用 Guard 中断连发并结束自动追击。手动版本、队形和其他任务优先。
- `held` 在 Sleep 入队时就记录工具持有关系，不证明原版已切换任务。纯算法继续提出 Hold，由 Runtime 仅在当前任务确为 Sleep 且目标为空时去重；因此请求超时而仍在 Attack 的单位可以重试 Sleep，避免把未观察到生效的请求永久当成已休眠。
- 无候选、目标消失或停用时用 Guard=5 清理持有。预测有运动样本后只预留确实接近、预计30帧内进射程的目标；静止、远离和带外目标不让射手永久等待。第一份运动未知样本可短暂预留。
- 关闭“提前休眠留枪”保留旧式有界攻击接近策略，建筑仍可休眠预留。关闭主功能/退出由现有持有清理流程恢复。

实际队列发送有延迟；已发射炮弹不能追回，12帧单发估计也不覆盖所有弹速、连发和未命中情况。不能据此承诺零过量射击。

## 2% 阈值与全局开关

原版自动索敌 `Techno::CanAutoTargetObject=0x6F7CA0` 先调用实际 SelectWeapon/GetWeapon，再在 `6F7D27..6F7D4F` 按目标 Armor 读所选武器弹头的 double Verses：

```text
fld   qword [Warhead + Armor*8 + A0]
fcomp dword [7F4E38]
fnstsw ax
test ah,41h
jne   reject
```

`7F4E38` 是 float32 `0.019999999552965164`。因此 double 的 0.02 大于这个 float32 阈值，精确2%的输入能通过该比较。新适配器只在自动获取候选处使用独立 double 0.02，沿用原版 x87 pop 和之后的拒绝分支，不写弹头 Verses、不改伤害或手动 Attack/CanFire 路径。特殊原版/MOD 自动索敌路径、反击和已有目标不由此保证全部被过滤。

全局开关默认关闭。单机启用时只过滤当前己方原生自动索敌，并过滤工具火控选敌；高级移动攻击原有 ≤2% 过滤保留。联机默认仅工具自动命令过滤。另设明确的“联机也修改原生索敌（实验性）”：此模式对**全部阵营**使用同一阈值，所有端须一致启用；若每端只改自己阵营，即使开关相同，各端模拟也会不同。该实验仍未实机双机验证，不声称单方启用兼容未修改端。

安装点 `6F7D44`，原6字节 fcomp 指令由统一 GameEntryManager 注册、签名与前后文校验、归属检查、恢复和冲突保护。适配器保留整数寄存器、EFLAGS和原版 x87栈效果，不申请第二套 Hook 管理器。关闭/退出恢复原字节；外部修改或依赖变化则拒绝覆盖。

## 原版 MegaMission 接管的当前状态

用户提出原版下令也走工具入口。目前仍**没有全量接管**：工具事件统一提交，原版 ClickedMission/QueueMegaMission 及规划流程保持原路径。某些增强鼠标操作只观察或按显式覆盖选项处理指定操作。原自动索敌不保证生成 MegaMission。

这几个相近名称对应不同层次，依据本次原版点击链受控执行及既有[任务事件研究](2026-10-26-selected-mission-modes.md)：

| 名称 | 已确认职责 | 是否本身代表联机传播 |
|---|---|---|
| `ClickedMission=0x6FFBE0` | 对象的点击任务入口；本次建筑/格子点击链从这里进入原版发送流程 | 需沿实际后端判断；本次链进入 QueueMegaMission |
| `MegaMission` | EventType `04` 的事件数据，含执行者、任务、目标、目的地等；`0x4C6860` 是填包构造器 | 事件构造成功本身不等于已入队或已传播 |
| `QueueMegaMission=0x646E90` | 原版事件发送后端；本次联机会话链构造事件并追加 OutList | 此处实证为联机发送，不能因直接调用函数就归为本地任务执行 |
| `QueueMission=0x5B35E0` | 对象模拟层的任务排队，写入对象 `+B4`；后续推进受 readiness 约束 | 函数本身不发送网络事件；应由各端消费同步事件后分别执行 |

可研究发送端接入手动任务仲裁，但必须保留参数、顺序和单次投递，处理工具自己调用 QueueMegaMission 的重入；规划追加与后续本机 DoList 推进不能误当成新的网络发送。构造器只填包、接收执行已进入模拟，均不能简单当作统一发送挂点。本版没有悄悄启用该接管实验。

## 验证索引

- `assist/tests/infantry_cell_capture.py`：37项原版动作/分派/发送与目的地门槛检查。
- `assist/tests/infantry_entry_runtime.py`：两种模式、四会话及边缘/覆盖等合成输入；真实构造器和最终 DLL。
- `assist/tests/missile_compute.cpp`：旧策略、预留、预计已射伤害、连发等待与释放，以及持有标记不能替代已观察到的 Sleep。
- `assist/tests/missile_sleep_runtime.py`：内/外计算、单机/联机队列的 Sleep/Attack/Guard、射后预留与手动接管。
- `assist/tests/low_damage_runtime.py`：最终适配器的精度边界、所属方、会话开关、恢复和冲突；只执行原版比较边界，非完整战斗。
- `assist/tests/experimental_patch_policy.cpp` 及最终 PE32/依赖/版本/Host-Runtime ABI 检查。

版本 `1.0.39-pre.3`；IPC 76，Source 增加预计已射伤害字段并增加四个共享设置/状态字段，Host和Runtime必须成套更新。最终日志在 `verification/review-139-pre3-entry-combat/`；未通过或未执行的项不能仅凭本文列表声称完成。

## 最终构建验证结果

最终 DLL SHA-256：`712bc93f269555bed113a319cfb544ed41cf0eed7e3d0eb6c440d2fc36b22e7e`。最终 DLL 的强制进入141项、任务模式100项、火控40项、低伤害适配器122项全部通过；另有原版建筑格动作/目的地37项、两组 portable 算法/补丁归属测试通过。最终 Host/Runtime 为 IPC 76，Shared 4165672 字节，PE32、版本、依赖和清单检查通过。火控40项包含 Sleep 未被观察到生效后的超时重试，以及异步结果返回前开始连发时延后自动 Guard。

测试夹具对 Aircraft::IsInAir 使用明确替身，并初始化独立的空玩家节点列表，避免读取捕获源进程的指针。事件构造器和被测 Runtime 仍执行真实指令；以上结果不包含完整游戏寻路、弹体命中或真实联机认证。
