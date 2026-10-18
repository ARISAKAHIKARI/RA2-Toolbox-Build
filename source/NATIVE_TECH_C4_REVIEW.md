# 原版科技生产与规划 C4 调查（1.0.9）

参考本地 YRpp 头文件与 `gamemd-spawn.exe`，EXE SHA256：c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a。此为 CnCNet 参考程序，不代表 LBW 或所有 MOD。

## 科技：可以复用事件通道，但自动预定本身不授予科技

自动预定只有在真实前置建筑落地、原生图标出现且 Runtime 完整 CanBuild 通过后，才提交原版 Produce。这解释了它不需要改写 Prerequisite。

本次沿 Produce 执行追到 `0x4C711B -> 0x4FA350`。开始生产在 `0x4FA42C` 调用类型的 FindFactory 虚函数（原版 `0x5F7900`），参数为 allowOccupied=false、requirePower=true、requireCanBuild=true、对应 House。该 FindFactory 内部 `0x5F7998` 传入 CanBuild 的是 **buildLimitOnly=true / allowIfInProduction=true**，而非完整前置判断。4 项探针中的两项执行了原版 FindFactory 分支，确认在夹具 full=false、limit=true 时能够找到工厂；limit=false 则找不到。

另一个实证：原版 `0x6AA711` 侧栏清理会针对不可造的已有 cameo 生成 Abandon/AbandonAll；只在外部展示、不向原版加入 cameo 的项目不进入这条清理循环。原版 `0x6A6140` 绑定生产工厂的函数也只查找已有 cameo，找不到则返回 false，并不自动 AddCameo。

由此可得一个值得继续验证的方案：**外部科技目录 + 统一 GameRuntime 构造原版 Produce + 外部进度显示**。它有机会绕过 UI 前置限制并避免图标清理中断，而不用清空本机 Prerequisite。不能据此宣布“全科技已实现”：仍需验证原版 Factory 从创建、扣钱、推进、完成到出厂/放置的全过程，以及阵营工厂匹配、上限、暂停/取消、偷科技、生产线切换和 MOD 校验。特别是未出现在原生栏的建筑，还缺少一条已经验证的放置交互链。

1.0.9 没有放松现有 Produce/ProduceReserved 的合法性约束，没有重新开启科技全开开关。

## C4：前端限制与实际执行检查不同

用户描述“普通步兵与谭雅一起规划，再移出谭雅，普通兵可爆破建筑”是待复现线索，不能用普通 Sabotage 命令的结果否定它，也不能直接推定任意步兵都能使用该漏洞。

已确认两段不同路径：

1. `0x51A4BF` 的步兵实际位置更新路径检查当前任务 17，然后在 `0x51A4D2..0x51A4E0` 读取 InfantryType+0xEC2（C4）。未开启会转向 `0x51A7F4`，开启才继续进入安装 C4 分支。原版机器码探针确认这个分支差异。这不是仅改变光标的判断。
2. `0x51F3E0` 还有 C4 类型标志或 Ability::C4（14）的攻击转换路径；存在这条能力判断，不应把一处 C4 类型检查说成全部规则。目标 CanC4、目标类别等也参与。

YRpp `TechnoClass::ProceedToNextPlanningWaypoint` 对应 Foot 实现 `0x4DE580`，继续进入 `0x4DC8C0`；该派发路径还会调用当前单位自己的鼠标动作查询。此次没有完成混合编组、谭雅移出、规划 token 残留及最终爆破的完整复现，因此没有加入“普通兵爆破”热键。

可研究的方向是重放双方原版都接受的规划事件序列；统一管理器可承载这种事件。直接在本机 NOP 爆破执行检查、修改 C4/能力或改写任务内存，会改变本机模拟结果，不能当作联机修复方案。若最终必须修改模拟判断，则需要各端一致规则环境，不属于单端通用入口的兼容扩展。

## 1.0.10 Beta 实现更新

本版已按显式测试要求加入开关。上述“1.0.9 未启用”的结论为历史版本状态。

- 科技开关使用新的 `[Experimental] NativeTechProduction`，默认 0，旧 `[Modes] Tech=1` 不会自动启用。兵、车、飞机支持从外部清单请求 Produce；Runtime 仍要求选中类型允许、对应工厂、供电与 `CanBuild(buildLimitOnly=true)`。不 AddCameo、不修改 Prerequisite；建筑因外部放置链未验证仍拒绝。界面提供四类工厂进度和所选类型的原版 Abandon。Abandon 已纳入统一构造适配器及同一生产仲裁，未私下入队。联机也允许显式测试，尚未验证扣钱、出厂到平台联机的完整过程。
- C4 开关、联机风险和准确补丁位置见 `HOOK_JUDGMENT_CHANGES.md`。当前是直接跳过 C4 标志读取的独立实验，不是混合路径漏洞复现。
- 新夹具执行编译 DLL/管理器、实际补丁和原版事件构造器，覆盖四种会话模式；模拟世界与部分原版查询仍由夹具提供。通过这些测试不代表真实双机同步一致。
