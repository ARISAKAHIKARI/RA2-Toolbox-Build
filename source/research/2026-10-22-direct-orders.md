# 直接发令：事件提交、接收执行和单位任务

## 问题与样本

用户问：统一入口是否只入队而不检查，能否继续深入并直接发令？

- LBW SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网 SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 测试夹具加载的 1.0.31 GameRuntime.dll SHA-256：`9c1984e673d25aa598d34c6f398631110fc18043f6d9296c1b95e8e77307666f`。本次没有修改产品实现或解除测试功能禁用。
- 先检索 INDEX、pre.10 路径点笔记、Scatter 笔记和持久 SQLite；旧笔记已经定位接收器及路径点分支，本次补充普通任务、出售和直接 QueueMission。

## 统一入口不是无条件提交

`assist/source/bridge.cpp::submitConstructed` 先调用 `authorizePlayerOrder`，再检查入口边界、构造参数、调用原版构造器，最后 `enqueue`。队列层仍检查对局、游戏线程、帧/请求状态、容量及预算。普通 Local 权限要求事件所属方等于当前本地所属方。

旧 PlayerTest / SellPlayerTest 例外还核对对象实际所属方等；当前 `playertest::Available=false`，不能据此宣称当前版本已经把敌方事件送入原版接收器。此前“入口一定提交、只是平台执行时拒绝”的说法不成立。

## 原版普通任务的执行链

| 地址 / 字段 | ABI / 含义 | 本次证据 |
| --- | --- | --- |
| `4C6CB0` | thiscall，ECX=Event 指针，无栈参数 | 原版事件分派；读取类型 `+0`、有符号 HouseIndex `+2` |
| `4C71CA` | 类型 4 MegaMission 分支 | 解析来源对象 `event+7`；`event+1D==1` 转路径点接收器 |
| `6386E0` | fastcall，ECX=单位、EDX=事件 | 普通事件清理旧规划路线；不是网络发送函数 |
| 单位 `+90/+6C/+81` | 存活标志 / 生命 / limbo | 普通分支检查；死亡对象受控负例拒绝任务 |
| vtable `+4A4`，坦克 `4DF0E0` | thiscall(Event*) | 解释任务，普通 Move/Attack 清理旧攻击移动缓存并返回任务号 |
| vtable `+1E8`，`5B35E0` | thiscall(mission, start)，ret 8 | QueueMission；原事件传 start=false，写单位待处理任务 `+B4` |
| vtable `+3C8`，坦克 `6FCDB0` | thiscall(target) | 原接收器设置攻击目标；本次终点只记录参数 |
| vtable `+480`，坦克 `741970` | thiscall(destination, bool) | 原接收器设置目的地，参数 true；本次终点只记录参数 |

`4C73B9` 调 QueueMission，`4C7467` 调 SetTarget，`4C747C` 调 SetDestination。前后还有无线电、原任务、Foot 状态清理，不能把完整发令简化成只写一个目标指针。

在普通 Move/Attack 的已测试分支中，事件 HouseIndex 写本地 0 或对象原属方 1，都能到达同一组任务、目标、目的地调用。没有在该受控分支发现像 Sell 那样的事件所属方与对象所属方拒绝条件。这不涵盖其他任务、特殊单位状态、未执行的虚函数内部或事件传输/接收层。

更深的 `5B35E0 QueueMission` 接收单位本身，不接收 HouseIndex。受控直接调用敌方单位 QueueMission(Move,false)，原生代码将 `+B4` 写为 2，当前任务 `+AC` 仍为 Guard=5。它操作的是**单位自己的待处理任务**，不是 OutList 网络队列；也不是让完整移动过程当场结束。它仍有任务状态限制，不能等同于无条件强制执行。

## Sell 的下一层仍检查所属方

- Event 类型 `0x16` 转 `4C6F12`。`4C6F3B..4C6F48` 读取对象 Owner 指针 `+21C`、Owner 的 HouseIndex `+30`，与事件的有符号字节 `+2` 比较；不相等直接返回。
- 合适的存活建筑通过后，在 `4C6FA2` 调用对象虚表 `+1A0`，参数 `-1`。
- 建筑虚表 `7E3EBC+1A0` 指向 `447110`，thiscall(Sell 参数)，ret 4。静态分析显示它还分支检查建筑 `+6E9`、类型 `+16C0` 等状态；常规路径进一步排任务并切换任务，另一路涉及原 Owner 的经济和对象移除。字段精确语义、完整退款/拆除流程本次不作结论。
- 因此可以定位到比接收器更深的 Sell 方法，但“绕过队列后就没有任何条件”仍然错误。本次没有运行 `447110` 的完整出售效果。

## 受控验证

脚本：`assist/tests/direct_order_execution_probe.py`。

结果：`verification/direct-order-execution/lbw.json`、`verification/direct-order-execution/zhanwang.json`，两样本各 8 例，总计 16 例 PASS：

1. 直接 QueueMission：敌方单位待处理任务由 -1 变 Move。
2. Move：事件所属方为本地 / 对象原属方，各一例；到达 QueueMission、SetTarget、SetDestination。
3. Attack：同上两种事件所属方，各一例；到达上述三个调用。
4. 死亡来源对象：普通事件接收器拒绝，无任务或目标设置调用。
5. Sell：事件所属方为本地，实际为敌方建筑，未调用 Sell。
6. Sell：事件所属方与敌方建筑一致，调用 Sell(-1)。

所有例均检查 OutList / DoList 数量为 0，单位和建筑 Owner 不变，CurrentPlayer 全局 `A83D4C` 不变。

执行的是原版事件构造器、分派接收器、普通任务转换、QueueMission 等代码。合成世界提供 ID 解析；无线电等部分辅助调用使用替身；SetTarget、SetDestination、Sell 为调用记录终点。因此结论是**接收执行路径和待处理任务状态成立**，不是完整寻路、开火、出售退款、实机画面或联网成功。没有真实双机验证。

## 方案含义与未决项

- 可以做不经过 OutList/DoList 的本机执行后端，原版接收器比散落调用几个底层方法保留更多清理逻辑。应在正确的游戏线程/仿真时机执行，并保留对象有效性检查。
- 直接调用本机执行函数不会自动把同一操作送到其他客户端。由本地调用语义推断，若只在一端改变参与仿真的状态，会产生不同步风险；不能将它作为“战网一定生效且不掉线”的证明或实现方案。
- 联机仍需确认同一个事件在各端、同一仿真帧被一致执行。当前无效功能先被工具自身禁用；解除该因素后的传输、调度、接收行为尚未完整追踪，不能提前归因于战网额外权限检查。
- DoList 的 `64C549` 所属方比较位于遍历 House 的上下文；`64C8E0` 的 CurrentPlayer 比较只在部分分支。不能孤立读取其中一条比较就断言“所有敌方事件被 DoList 拒绝”。
- 不需要靠切换 CurrentPlayer 来定位这些执行方法；切换指针会引入另一组 UI/玩家上下文问题。

命名交叉参考：上游 [YRpp MissionClass.h](https://github.com/Phobos-developers/YRpp/blob/master/MissionClass.h)、[TechnoClass.h](https://github.com/Phobos-developers/YRpp/blob/master/TechnoClass.h)、[ObjectClass.h](https://github.com/Phobos-developers/YRpp/blob/master/ObjectClass.h)、[NetworkEvents.h](https://github.com/Phobos-developers/YRpp/blob/master/NetworkEvents.h)。精确地址及结论以以上两个样本和受控结果为准，未发布私有游戏文件或原始转储。
