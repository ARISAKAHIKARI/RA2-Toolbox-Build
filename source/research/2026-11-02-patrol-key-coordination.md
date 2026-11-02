# 自定义巡逻键与原生规划提交的协调

问题：巡逻手势允许绑定任意键和鼠标，不能继续把物理 Z 当作原生规划键，
也不能在重绑定后依赖模拟按 Z 或写入 PlanningMode。

调查前检索研究索引、`2026-11-02-mo-command-gestures.md`，并读取既有
`2026-10-22-pre10.md`、`2026-10-26-network-event-audit.md` 与
`2026-10-31-native-planning-render-hotfix22.md`。本次没有新的原始 EXE、
反汇编或原生机器码执行，沿用原有映像准入，不新增 EXE 哈希支持声明。

旧 `assist/tests/infantry_waypoint_runtime.py` 的 ordinary/engineer/spy/c4
案例没有设置物理 Z 或 Display PlanningMode；它们执行原生接收器
`0x4C6CB0`，追加 MegaMission `+0x1D=1`，再显式 PlanCommit `0x2B`，
观察下一事件 `+0x1D=2`。native_key 对照设置 `Display+0x11B3`，仅改变
工具是否自己再发 Commit。旧 pre.10 已记录：不处于原生编辑时显式提交，
处于原生编辑时由原生松键提交。这里是读取已有受控证据，没有重新运行
这些依赖私有原始 EXE 的测试。

实现是专属 `mo_patrol_planning_bridge.h`：

- 使用现有 `nativekeys::load()` 只读读取 `0x87F680` CommandClass 热键表，
  解析真实 `PlanningMode` 绑定；不调用陌生 GetName，不猜字母。
- 巡逻自己的键只负责手势。原生规划未激活、实际原生键也未按住时，
  闭环就绪后经 `submitConstructed()` 发一次标准 PlanCommit。
- 玩家正在使用原生编辑时，记录实际主键及所需修饰键。构图期间暂存其
  原始 KeyUp/SysKeyUp，等图就绪且实际主键松开后，原样 PostMessage。
  原生 UI 发送器拥有这次 Commit，工具不额外构造第二个 House 级 Commit。
- 先松修饰键不会越过仍按住的原生主键；主键重按后，保留最后一次释放。
  自定义捕获之后才开启原生编辑的情况也纳入协调。
- 原生编辑已激活但真实绑定/按住状态无法确认时，拒绝巡逻捕获；不退回 Z。
  过期 match、House、窗口身份或前台应用的延迟释放不投递到新上下文。
- 不修改游戏代码段、PlanningMode、PlanMgr、Token、任务或随机数。
  规划事件依旧走原有 MegaMission/PlanConnect/PlanCommit 与 DoList/OutList。

验证：`assist/tests/mo_patrol_planning.cpp` 包含生产协调头，并由真实
`nativekeys::load()` 解析合成 CommandClass 表。在替代 Win32、内存和
中央事件网关边界下，检查无物理 P/Z 的自定义捕获、Ctrl+P 分序松键、
无重复 Commit、主键重按、未知绑定拒绝、过期上下文、投递失败重试，
以及捕获后开始原生编辑的分支，G++ C++17 编译执行通过。

这些检查不执行 Windows 消息队列、原生接收器、寻路或联机。重放成功
表示原始 UI 释放消息已投递，不代表该 Commit 已被原生队列接收或单位
已开始巡逻。PlanCommit 仍然作用于整个 House；混用其他未提交的原生图、
构图中途取消及 MOD 共存保留原有实机验证边界。
