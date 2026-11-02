# 快速占位接入现有队形

基线是当前 `1.0.39-pre.7` Hotfix33 源码。实现前查阅研究索引、
`2026-10-25-formation-shape-switch.md` 与
`2026-10-24-fa2-sticky-drop-137-pre8.md`。本次没有新增游戏函数或 ABI
推导；没有取得新游戏 EXE，沿用现有映像准入，未新增支持的 EXE SHA-256。

参考固定为 `MagicShiba/MO_ExperienceEnhancements` 提交
`33ed558bd38f36b5012eddcc9c0fb109cc68e38a` 的
`src/order/SmartOrder.cpp`。独立实现其点按螺旋配额、曲线按弧长重采样和
空地分组行为，复用工具箱原有输入边界、队形预览和 OutList 事件调度。

- 开启队形后，裸中键按住预览；点按在松开位置螺旋分配，拖动按最多64个
  世界坐标节点排位。路径满后保留最新尾节点。Ctrl/Shift/Alt/Z 修饰中键
  不归本模式，按住途中追加这些键会取消快速占位。
- 步兵每格至多3个，车辆和航空每格至多1个，三类配额独立。地面、空中
  曲线各自覆盖完整弧长，同类内稳定最近槽位分配减少交叉。
- 航空类为 Aircraft，或既有类型字段 `MovementZone +0x5B4 == Fly(9)`、
  `SpeedType +0x67C == Winged(4)`。这些偏移来自既有研究，未照搬参考库
  自述尚未确认的 SpeedType 1596 偏移。
- 地面预览沿用永久障碍过滤；航空落点只要求有效地图格。曲线落点按半径5
  寻找有效且配额未满的格；点按搜索逐圈向外。没有调用会改变模拟状态的
  路径探测、ClickedMission 或对象命令函数。
- 新模式通过现有 `formationPreview.shape == 6` 标记，仅作为临时标签。
  六种可配置队形、Policy/Preview/Shared 的结构和 wire ABI 都不改变。
- 提交仍进入原有 FormationJob、手动版本取消及任务仲裁，发送普通 Move
  MegaMission。正常手动左击可平移记忆槽位；快速占位不增加朝向接近格，
  不直接修改单位位置、任务、规划队列或随机数。

验证：`assist/tests/formation_quick.cpp` 在本地 G++ C++17 执行通过，检查
256单位混编配额、障碍过滤、弯曲轨迹真实弧长位置、空地独立覆盖、无有效格
拒绝分配及节点满载尾段保留。生产 `assist/source/bridge.cpp` 以 Zig 0.13.0
的 x86 Windows 目标编译成功。上述不是实机寻路、最终队形或联机验收。

未决：游戏动态拥堵/地形通行仍由原引擎决定；步兵同格的原生子格选择不由
工具箱强制指定。中键被快速占位独占时，不应再绑定另一个裸中键命令。

## 宿主轮询与 Runtime 输入的单一所有者

集成审查发现 `host_game_input.h::pollPointer()` 通过
`GetAsyncKeyState` 在中键按下时就生成 `PointerPress(operation=1)`，
`pointer_host.h::servicePointer()` 再按既有功能配置下发 mailbox。它不读取
游戏的 MSG 是否被吞掉。因此只在 Runtime 过滤中键，不能阻止旧的
Ctrl+中键飞机分配等功能另发一条命令。`hotkey_host.h` 的 sampledOnly
鼠标快捷键也有同类入口。

新增纯判定 `middle_input_owner.h`，直接复用 MO Runtime 的
`moorders::claimsMiddle()` 和快速占位的 `quick::claimsMiddle()`：
Ctrl且不含Z由模糊下令拥有；无Ctrl的Z由巡逻拥有；无Ctrl/Z的Shift由区域
分配拥有；无这些修饰键的裸中键在开启队形后由快速占位拥有。Alt和Ctrl+Z
不被新功能占用。某模式关闭时，该组合仍归旧输入处理。

宿主在实时按键已构成独占手势或 Runtime MO 预览活动时停用旧 pointer
采样、清除待发匹配；出队时还检查该 `PointerPress` 自带的原始键快照，
堵住先采样、后发布预览的先后差。键盘快捷键批次同步停止，避免鼠标
sampledOnly绑定或延迟释放另发命令，旧命令光标提示也在预览期间撤下。
判定按已开启功能预留手势，目标能力校验失败不会自动变成另一项旧操作。

`assist/tests/middle_input_owner.cpp` 穷举256组功能/队形开关/修饰组合，
验证只有中键被预留，并让默认 Ctrl+中键飞机分配走真实旧 resolver，
确认预留后零条旧路由。`formation_quick_bridge_test.py` 编译并执行生产
`formation_bridge.h`，在替代 Win32、内存查询和命令提交边界下验证：
25步兵提交及记忆平移后仍为9个格且每格不超过3人；新增修饰取消后中键
释放不提交；点按的释放位置生效。此测试不表示真实游戏事件或联机验收。

使用最终宿主构建参数（x86_64-windows-gnu、RA2_UNIFIED、
RA2_GPU_FAULT_GUARD、TacticalEntry、suite include）编译生产
`assist/source/main.cpp` 成功，仅保留原有素材缓存和 shared_ptr 的3条警告。
