# v1.0.30-pre.5 实现与验证

基线：v1.0.30-pre.4 / 2b756f69cb8ec984b6a44b88b8612028e3999dda。文件版本 1.0.30.4，Assist 58、Runtime 4、入口管理器 3。Assist 数据结构已改变，宿主与 DLL 必须成套更新。

## 重复转向的原因

pre.4 将每个新弹体作为绕过 12 帧等待、重置三次移动预算的条件；选点时还排除上一目的地。连续弹体因此可能在上一 Move 尚未执行、车体转向或未加速时不断更换落点。威胁消失 6 帧后自动恢复旧 Attack/Guard 也没有等待实际移动完成。旧测试仅检查事件和静态落点，未覆盖延迟执行下连续新弹体与持续转向/移动。

## 原版路径与边界

依据提供的两份 EXE 及 [YRpp EventClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/EventClass.h)、[UnitClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/UnitClass.h)、[FootClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/FootClass.h)、[Phobos Aircraft hooks](https://github.com/Phobos-developers/Phobos/blob/develop/src/Ext/Aircraft/Hooks.cpp)核对：

- 飞机开火后调用 Cell::ScatterContent（0x481670）；格内精英、PlayerScatter、SCATTER 能力及 House IQ 等条件影响原生触发。这不是任意三星单位对所有攻击的通用无敌机制。
- Scatter 事件类型 0x07，原构造器 0x4C65E0；仅携带 Whom / Techno NetID。接收端 EventClass::Execute 的 0x4C7859 分支解析对象，检查存活、非 Limbo、Foot 标记等，写原生散开标记 +6B2，然后调用 Scatter 虚函数 +174，参数为默认坐标、true、false。
- UnitClass::Scatter 0x743A50 检查可散开、locomotor、任务、车体朝向计时器、Destination、部署状态等。PrimaryFacing::IsRotating 在 0x4C9480；已有目的地且不允许覆盖时直接返回。落点及 SetDestination 由该原版函数处理。步兵通过同一事件接收端调用自己的 Scatter 实现。
- 飞机原生 Scatter 的来源坐标是飞机位置；事件 Scatter 没有这个负载，而是默认坐标。原生单位函数对默认坐标与非默认坐标有不同选点分支，不能把“使用同一函数”称为“复刻所有三星行为”。同步队列还存在执行延迟。

工具仅提交正常同步事件，不在本地直接执行 Scatter、消耗随机数或修改星级/能力。Scatter 总是走中央构造/入队路径，不走只验证了建筑 Repair/Sell 的实验 ClickedEvent 后端。中央入口继续校验本地所有权、单位种类、存活、会话、时效、队列和单位仲裁。

## 一轮散开只下令一次

删除自选两格落点、弹道评分和所有躲避恢复任务。维护“已入队 → 观察到执行/移动 → 停稳”的状态。新弹体在本轮活跃期间只加入已见历史，不覆盖目的地；目的地、排队移动、车体转向或实际坐标变化阻止结束。观察到移动/原版标记变化后至少两逻辑帧稳定才释放任务。未观察到执行的入队最多等待 300 逻辑帧，再记为未确认过期，不补发旧任务。

对原来已经移动/转向的单位记录当前弹体，避免停下后再躲同一发。手动命令、对象/所有者变化、单位失效立即取消本地任务；已入原版队列的事件不被重写。关闭功能停止新增事件，正在执行的原版移动自然结束。每单位保存最近 32 个弹体 ID；最多 128 个任务/短期历史，按预算轮换；已有活跃任务不会被淘汰。连续碾压威胁可在实际散开完成后开始下一轮。

散开活动期间续持 Dodge 仲裁租约，防止低优先级自动任务在长转向/移动中抢发命令。Burst 任务仍按既有优先级让位给 Dodge，不追加固定延时恢复攻击。原版自身保留或结束何种任务，由其 Scatter/locomotor 行为决定。

## 验证

见 verification/pre5/summary.json。战术用例运行本次实际 x86 DLL，覆盖单机/联机队列、宿主/Runtime 两个计算后端：连续 20 个新弹体、等待执行、原地转向、持续移动、停稳后新弹体、原生已有目的地/排队移动、关闭功能、手动取消、同帧较晚开火、非本方、治疗弹、过滤、超时及实验对象后端。检查仅提交 Scatter，且没有旧 Attack/Guard 恢复或本地单位/代码改写。

新增原版接收端测试实际执行 EventClass::Execute、UnitClass::Scatter 和 PrimaryFacing::IsRotating，确认 true/false 参数、原生标记、转向/目的地/部署/存活等门限，以及正常/转向计时结束后才进入原版选点分支。对象解析、COM locomotor、地图查询和最终 SetDestination 使用受控替身，未执行完整寻路与弹道物理。高刷回归确认 Assist 结构变更后尾部字段仍可用；入口、宿主及 PE/ABI 静态检查通过后才打包。

未进行 Windows 完整游戏或双机联机验证；不能将这些测试当作实机躲弹成功率。没有新增游戏 Hook、平台禁用或科技/地图功能限制。
