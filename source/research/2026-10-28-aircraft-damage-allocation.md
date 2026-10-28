# 飞机预计伤害分配与单目标限额（2026-10-28）

## 问题与依据

新增两种显式飞机分配策略，同时保留原配额模式1/2：点击目标周围五格的敌方地面战车按预计伤害120%改分；单目标只派出足额飞机，成功派出的飞机取消选中，剩余对象不下新命令、不清任务、不取消选中。

先检索 `INDEX.md`、`2026-10-23-fire-formation-137.md`、`2026-10-26-entry-fire-control-pre3.md`、`raw_combat.h`、`weapon_policy.h`、`missile_bridge.h` 与同一持久 SQLite 笔记（本地只读版本74）。已有 missileEdge 并非完整伤害模拟：使用原生选武器后做 Damage×Verses 的单发估计。新功能复用其已知武器布局与兼容规则，不能把基础 Damage 或 effectiveness 单独当作实际扣血。

支持样本仍为 LBW SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、战网 SHA-256 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。本笔记中的选择/伤害字段是复用既有布局与生产代码；唯一新核实的原生取消选择调用另见 [单对象选择研究](2026-10-28-aircraft-selection.md)。没有新增伤害执行器补丁。

## 策略与纯分配

设置页的“飞机分配策略（1 / 2 共用）”提供三个选项，默认0，INI为 `[Cooperative] AircraftAllocationPolicy=0/1/2`。

- 0：原受击数量配额，原1/2两种优先配额与最大数量均保留。
- 1：`AirDamage=4`。点击时固定世界XY中心，半径1280 leptons（5×256），采用XY圆且含边界，不受随后镜头滚动、目标移动或屏幕边界改变。只候选有效敌方 UnitClass（kind2）且非空中/被操控战车；不把步兵、建筑、Aircraft、盟友、中立方纳入。桥面/高度不改变XY半径。
- 2：`AirSingle=5`。只考虑点击的敌方目标，可为其他兼容战斗对象；按每架预计有效伤害从大到小选择必要子集，相同估计按ID稳定排序。预计累计达到当前HP×1.2即停止；绝不把剩余飞机回退投向已覆盖目标。

五格模式先处理选择更少的飞机，并在未覆盖候选中优先点击目标、随后按距离/ID选取。决定是否覆盖时先看已经承诺的累计伤害：低于120%允许这一架跨过阈值；恰为120%视为已覆盖。只有这架飞机没有有效且兼容的未覆盖候选时，才允许继续投向已覆盖目标；不兼容/零伤害目标不能阻止回退。已覆盖回退按覆盖比例、点击目标、距离及ID稳定排序。该模式忽略旧数量配额，避免把120%策略悄悄受旧maximum限制。

`cooperative_plan.h` 仅处理不可变值，不调用原生选择器、RNG、路径或任务API。拒绝重复源/目标身份、错误矩阵尺寸、越界位置、非正HP、NaN/无穷或无效预计伤害。

## 预计伤害的具体范围

`cooperative_damage.h` 使用现有 `planner::weaponIndex` / `GameView::combat` 选择槽，包含主副武器、IFV当前槽、阶段武器与精英武器数组。复用 Weapon `+A4` Damage、`+AC` Warhead、弹头 `+A0+Armor*8` 的原始double Verses、Weapon `+9C` Burst、类型 `+684` 弹药限制及对象 `+2FC` 当前弹药。估计一个完整的普通连发 volley，连发数受当前弹药保守限制，不把全部未来补弹/多次出击计入。

沿用 `unit_status.h` 已确认的 Foot `+160` 火力箱double与目标Foot `+158` 护甲箱double，估计 `Damage × double Verses × Firepower / Armor × shots`；建筑不读取Foot箱子字段。缺失、非正、非有限或超界乘子直接拒绝。正在部分连发、无弹药、治疗、心控、超时空、寄生、磁电/移动器类非普通HP伤害武器不伪造一个数值预算。其他原有目标保护、武器对空/对地、护甲兼容规则保持。

Host读取和Runtime内部计算使用同一纯读取/估计器。最终游戏线程发令前还用已验证 `GuardedCombatWeapon` 的实际 SelectWeapon/GetWeapon 查询，要求原生槽/武器等于估计槽/武器，且当前估计等于订单快照；变化或不支持的MOD选择器保守拒绝该源，不把另一把武器当作旧伤害发出去。距离只限制候选区域，不把“当前在武器射程外”误判成不可下Attack；原生命令仍负责接近、开火与命中。

这些是预计普通volley伤害，不是原版ReceiveDamage完整仿真：未覆盖任意MOD选择器/扩展弹头、所有老兵/阵营/难度额外倍率、溅射和多目标命中、弹速、闪避、拦截、后续补弹或实际命中归因。即使预算足额也不保证实际击杀，不保证零过量攻击。

## 成功提交、重规划与保留未派出者

订单的 `expectedDamage` 固定为该次快照预计值。Host仅依据实际 acceptedBits、Runtime仅依据实际成功 nativeMission 累计预留；队列满未处理和晚验证拒绝不计入。新计划使用原Request源池减去真正processed源，保留上一计划未选中的飞机作替补，不能只保留旧订单后缀而永久丢掉未分配者。Host与Runtime共用 `reserveAccepted` / `retrySourcePool`；保留原点击时戳、serial、身份token和中心。

Runtime伤害模式在新模拟帧重采样HP。单目标还有独立成功提交累计守门：即使旧计划尚在新鲜度窗口内，目标HP降低后已经派出估计足额，就拒绝随后飞机，不取消其选中。已接受预留在这个有界请求期间不因源死亡、手动接管或目标HP变化而倒扣，属于保守快照预留，不是完整在途伤害账本；新请求/新对局清空。目标身份/type变化不转移预留到另一个对象。

单目标收集阶段跳过 `pointer_bridge` 与 `newCooperative` 原先面向全选区的 `cancelNative`。后者会释放Sticky、清自动任务、改变手动版本和lease，不能用于尚未确定派出的候选。派出时仍尊重本地Sticky保护，失效/受保护源可让未使用候选替补。

单目标的显式用户任务使用网关的窄范围takeover参数：原事件确实物理入队前不改变旧lease/回执；成功后才替换旧lease、取消旧receipt、记录新Cooperative lease，保留手动token。随后只清这架飞机旧的工具任务元数据，不能调用会顺手删掉新lease的 `cancelOreJob` / `cancelNative`。最后再次核对ID/指针相同，调用已核实的原生单对象Deselect。失败/队列满/被晚验证拒绝的飞机不进入这些路径。

## 输入接管与ABI

中键/侧键等独占显式增强输入可直接使用单目标模式。单目标不支持Shift排队（包括把按住键配置为Shift）：符合手势的原点击仍吞掉，但指针处理拒绝派出，保留所有任务和选区；其他模式的Shift行为不变。左右键必须启用“优先执行增强指令”并真实吞掉原点击；没有suppressed证据，Runtime不假装事后追加几个Attack就能限制已给全组发送的原Attack。

原消息过滤器仅对本功能已配置的单目标有效飞机攻击手势开放对象接管。混合选组含己方飞机时接管整次原点击，其余成员仍留给选择/分配过滤。队列满、指针信箱忙、无有效武器或选择helper不可用也不能退回原全组Attack；吞掉并不提交，保留任务和选中。超过512可处理源池同样保守吞掉并拒绝。DOWN/UP及双击第二次DBLCLK/UP成对处理；非本模式双击保留旧行为。未启用原生接管、没有有效飞机或非本功能手势维持普通输入。

IPC升级80；Host与Runtime必须成套。Assignment增加double expectedDamage，Request末尾增加centerObject保护点击对象身份；damage模式复用cameraX/Y为点击世界中心，其他模式仍是原点击时镜头。Shared尾部增加aircraftAllocationPolicy。更新ctypes与Windows ABI验证到80/v080。

## 回归与证据等级

- `assist/tests/aircraft_damage.cpp`：真实纯分配器的120%等号/跨阈值、每架飞机兼容未覆盖候选、覆盖回退、圆边界、源伤害差异、单目标停止、原源池替补和已接受预留；旧Enter/两配额模式回归。
- `assist/tests/aircraft_damage_reader.cpp`：生产GameView/reader/allocator的合成内存世界，圈内屏幕外、圈外及圆对角边界、敌我/中立/类型/空中/操控/保护过滤、精英/副武器/IFV/阶段、精确double Verses、弹药/连发/箱子乘子及无效伤害；单目标建筑与旧屏幕模式隔离。
- `assist/tests/auto_settings_ui_test.py`：生产UI事件与真实INI读写表达式，0/1/2、默认、越界及即时同步保存，standalone/RA2_UNIFIED均通过。
- `assist/tests/aircraft_damage_runtime.py`：实际最终DLL的Host/Runtime计算与原生构造/取消选择链、部分队列、未派出保留和输入接管（结果须以最终哈希报告为准）。
- 既有 `cooperative.cpp`、`robust_cooperative.cpp`、`tank_bunker.cpp` 全部通过。新单对象Deselect的两样本136项原函数及38项生产helper验证见独立笔记。

以上portable/合成世界/受控x86证据不等于真实Windows点击、完整弹体物理或双机联机通过。私有EXE、原始函数字节和内存转储不得加入仓库或发布包。
