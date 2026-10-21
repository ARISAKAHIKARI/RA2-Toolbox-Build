# 1.0.29 实现与验证

基线：1.0.28 正式版。本次处理“卸载成功但工程师不占领”与防空导弹拦截。不是实际联机验收。

## 智能卸载

原版 Foot 点击路径 0x4D7731–0x4D773A 发送 Capture(8)，目标建筑放入 Destination；原有构造参数与此一致，无需改成直接任务写入。

旧状态机存在两个覆盖不到实战过渡的点：普通卸载 Move 仅保留 180 帧；工程师一旦取得目标/目的地便结束跟踪。现在仍有总任务 900 帧的有限期限，普通卸载散开可在此期限内等待，非部署乘员需连续两帧坐标稳定。Capture / Enter 寻路中保持观察；被临时 Move 或 Guard 中断时等待稳定并重新查询原生 MouseOverObject，再发送同类原版事件，最多六轮重试。手动版本变化、Stop、死亡、换所属、功能关闭或换局终止任务。取得目标只记为 Observed，不声称已经占领或击杀。

部署型逃生流程继续使用原有部署状态机；本版没有把逃生转换成直接写任务或写乘员位置。

## 防空火控

- `missile_bridge.h` 在现有 Runtime dispatcher 中执行，有扫描游标与上限（每批 64 个射手、64 个导弹、最多 8 项建议）。超限对象轮换观察，不创建独立线程或入口。
- 默认 10 Hz，可配置 1–60 Hz；每个逻辑帧最多采一次。大规模场景单个对象的实际周期受轮换和事件预算影响。
- 原版 `TechnoTypeClass::MissileSpawn`（+0xD68，INI reader 0x714F37）标识导弹单位；要求敌对、存活、在地图上、可选为攻击目标、非隐形/铁幕且匹配用户类型过滤。
- 射手必须己方、在场、有对空武器和弹药；不打断移动、载运、部署、高级移动任务、正在连射及已锁定渗透者的防偷点杀。建筑通过原版 IsPowerOnline (+0x350) 检查。
- `GuardedMissileWeapon` 在游戏线程调用虚表 SelectWeapon(+0x2E4)、GetWeapon(+0x3F8)、IsCloseEnough(+0x3A8)。没有根据平面半径预先排除高空目标，也没有另写一套近似射程公式。当前两个用户程序该距离入口为 0x6F77B0，核心 0x6F7220。
- `missile_compute.h` 只消费值类型观测与合法边矩阵。优先保留足量已有锁定，给未覆盖导弹分配射手；优先威胁己方的导弹。伤害为名义单发伤害乘装甲比例的估计，不是飞行中弹药的命中保证；不修改生命值或发射弹丸。
- 已入队但尚未观察到的命令短期保留，至少等 12 帧和 350 ms 才允许超时重试。已在射击同一目标时不重复下令。

## 两种计算位置和唯一执行入口

| 步骤 | 工具箱计算 | 游戏内计算 |
|---|---|---|
| 原始观测、原版武器/射程查询 | GameRuntime | GameRuntime |
| 火力分配 / 卸载目标决策 | EXE 内共享纯计算代码 | DLL 内同一份纯计算代码 |
| 新鲜度、身份、手动版本、目标及距离复核 | GameRuntime | GameRuntime |
| 公共预算、任务仲裁、原版事件构造与入队 | GameRuntime | GameRuntime |

外部结果采用序号保护的不可变批次，原始 match/tick/frame 不在传递时刷新；最大 150 ms / 6 帧，过期丢弃后重新采样，不执行迟到指令。每局锁定计算位置，防止两边同时执行。控制项 ID 整组移至 9000 段，避免第 11 个计算选项与小地图位置 850 冲突。

普通战术、协同下令和拦截共用 planning budget 的三份配额以及既有总事件上限。最终仍调用 nativeMission，复用演员帧仲裁、跨帧任务仲裁、SP DoList / MP OutList 构造和回执；没有新建私有事件队列或 hook。既有其他功能允许的游戏修改清单没有增加。

## 验证

- `release129_runtime.py`：每份用户程序 72 个场景，覆盖 SP/MP 会话、两种计算位置、长散开、滑步、寻路中断重试、手动停止、开关、原生事件内容；火力分配、高度拒绝、友军/非导弹/观察者排除、断电、防偷冲突、过期/撕裂/目标死亡/提交时射程变化等。
- `missile_native_range.py`：每份程序 6 个用例直接执行原版 0x6F77B0 / 0x6F7220 算术、开方与转换，验证高度、AirRangeBonus、最小射程。对象 getter 与地图/障碍查询使用受控替身，不是实际地形测试。
- `missile_compute.cpp`：ASan/UBSan 检查分配、已有锁定、待观察命令、容量和三类共享预算；本环境禁止 LeakSanitizer 进程探测，因此关闭 leak 检测。
- 快捷键宿主回归、PE32/import/版本/Shared ABI 与递归架构审计。

真实 Windows 视觉、实际路径物理与真实平台双机同步尚未验证。测试在人工世界中执行真实 DLL 和原版构造，不等同于完整游戏仿真。

## 参考

- [YRpp TechnoClass](https://github.com/Phobos-developers/YRpp/blob/master/TechnoClass.h)：原生虚拟武器选择、射程接口。
- [YRpp TechnoTypeClass](https://github.com/Phobos-developers/YRpp/blob/master/TechnoTypeClass.h)：MissileSpawn、AirRangeBonus。
- 用户提供的两份游戏程序，仅用于本地原版边界核对，不随源码或 Release 分发。
