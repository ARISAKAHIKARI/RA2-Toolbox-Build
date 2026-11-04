# 原生持续授予超武：每帧重复、永久标记与联机边界

问题：游戏关闭超级武器时，是否能像科技全开那样每帧 AddCameo，或每帧调用地图“持续授予”来获得可联机使用的超武？用户明确频率为每个游戏帧，不是每秒。

支持私有 PE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。先查询索引、SQLite、禁用超武及地图动作／网络事件笔记；固定参考 CnCNet/YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 TActionClass、SuperClass、HouseClass、EventClass、GeneralDefinitions。最终分支结论来自该 PE 原始 x86 的控制执行。本轮没有游戏进程、网络同伴、弹头或发射效果。

## 纠正“普通 Grant”等于地图“持续授予”的假设

前轮 17 例只执行了普通／一次性 Grant 及维护片段，不能据此把普通实例被回收的结论套用到完整地图动作 34。新验证执行了地图动作本体，发现额外的永久标记。

| 入口／字段 | 原生行为与证据 |
| --- | --- |
| `TActionClass::GrantPermanentSW 0x6E1C40` | thiscall，四参数，RET 16；Action `+90` 取超武索引，House `+258` 取 Supers |
| `Super::Grant 0x6CB560` | thiscall，三个 bool，RET 12；动作 34 调用 `Grant(false,false,false)` |
| Super `+60 CanHold` | 动作 34 随后置零，这是普通 Grant 缺少的永久保留行为 |
| Super `+6D/+6E/+6F` | Present / OneTime / Ready；持续授予并不等于每帧设 Ready |
| Super `+30/+38` | 充能 timer 起点／时长；已 Present 时 Grant 于 `0x6CB569` 提前返回 |
| `0x50AF47` | 既有实例维护；未战败且 CanHold=0 或 OneTime=1 时跳过普通来源／回收检查，走 `0x50B1AC`；普通实例走 `0x50AF7E` |
| `0x6A6300` | 本地当前玩家的 AddCameo(31,index)，另有 tab/redraw 呈现调用 |
| `House::Fire_SW 0x4FAE50 → Super::ClickFire 0x6CB920` | ordinary 非 ChargeDrain／非 PostClick 路径，`0x6CBA7D..0x6CBA98` 检查 timer 已启动、Present、Ready；通过后才到 Launch `0x6CC390` |

在 SWAllowed `0xA8B263=0`、Disableable 类型的合成实例中，第一次完整动作 34 设置 Present=1、OneTime=0、CanHold=0，并启动 timer；普通来源回收片段随后会跳过该实例。战败 House 不享受该保留路径。

在接下来的 **180 个连续游戏帧**，每帧调用一次相同动作，Super 对象／计时器保持不变。结果证明已授予时重复调用不会重置 timer；本轮未运行完整每帧充能 Update 或发射后整个生命周期，不能把不变的合成计时器当作完整真实充能测试。

因此本机方案可以每帧检查、缺失时补授予；没有必要为保留已有实例每帧无条件反复调用整段动作及呈现刷新。只 AddCameo 只能补本地侧栏，不能代替 House/Super 的授予和就绪状态。调用原生函数本身不需要硬件断点；这也不是已实现的产品开关。

## 硬件可改的门与同步范围

已定位的普通建筑授予门 `0x50B298`、既有建筑来源维护门 `0x50B078` 会读 SWAllowed／Disableable，是进一步验证硬件拦截的候选。更改本机读取结果只能影响本机执行；本轮没有为这些站点验证完整 VEH/寄存器方案、分配槽位或新增安装逻辑。改变地图触发条件、动作参数或单方直接执行动作同样不自动让其他客户端执行。不能列出未验证的任意地图逻辑并称其已联机可用。

地图动作 33／34 与网络 EventType 分属不同编号空间；`SpecialPlace=0x12` 是使用已有超武的事件，不是授予广播。已验证普通 `Fire_SW` 路径在接收方自己的 Present／Ready／timer 不满足时会拒绝进入 Launch。提升本机 Grant 或 AddCameo 的频率不改变这一点，也不能从单机绕过推导联机同步。

要各端执行同一授予，需要共同地图的原有触发逻辑按一致模拟状态执行，或所有客户端的配套同步实现。只在本机改变触发判断、Grant、CanHold 或充能，均没有在本轮证明能跨客户端同步。科技全开允许本机选择后发送正常生产事件的机制，不能直接当作“本机超武状态可以独立授予”的依据。

## 可复现证据

`assist/tests/superweapon_periodic_grant_native.py`：12 cases PASS，含禁用局首次持续授予、逐帧 180 次不重置、四组保留门、当前玩家侧栏调用，以及五组 ordinary House::Fire_SW 的发射准入。摘要为 `evidence/2026-11-04-superweapon-per-frame-native.json`。

除单独一例明确替换 AddCameo/tab/redraw 呈现为 sink 外，Grant、永久标记、保留门及 Fire_SW/ClickFire 均执行原始指令；在 Launch 入口停止。PostClick、ChargeDrain、发射后的完整生命周期、真实倒计时 UI 与禁用超武的双机对局尚未覆盖。没有新增强制授予开关，也没有将地图动作伪装成网络事件发送。
