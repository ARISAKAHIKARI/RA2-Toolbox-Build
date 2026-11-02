# MO 手势键位与原生规划键解耦

本记录更正同日初次迁移中固定 Ctrl/Shift/Z/中键的输入限制。实现前查阅
研究索引、MO 命令、快速队形与既有原生 Planning 记录；持久 SQLite
由整合任务统一更新。本次没有新增游戏指令地址、字段偏移、映像或
支持的 EXE SHA-256，属于工具箱输入策略和既有事件接口整合。

`moorders::GesturePolicy` 为四个 `uint32_t keys` 加四个 `uint32_t buttons`，
索引依次为模糊下令、区域分配、巡逻、快速占位。默认键仍是 Ctrl、Shift、
Z、纯鼠标，默认鼠标均为中键，但均可改为其他键盘 Hold/Double-Hold
chord，以及中键、两侧键、左键、右键；mouse=0 单独禁用。键盘字段0
表示只使用鼠标，不再特殊排除某个写死的规划字母。

Runtime 的 `moReadGestures()` 和宿主的 `runtimeGesturePolicy()` 均通过
Shared 的 `moGesturePolicySeq/moGestures` 获取同一策略。执行匹配使用
已有 `keygesture::matchesHold`；Host 与 Runtime 共用
`moorders::resolveGesture()`，更具体的 chord 优先。完全重复配置和同时
等优先级的多个候选均不发令；宿主仍预留该冲突手势，避免变成另一条旧
增强命令。显式 Press/Release 不是连续鼠标授权，会被标为无效。

捕获记录实际鼠标键和完整策略。松开自己的左/右/侧键正常提交，其他鼠标
键按下才取消。键盘 Hold 失效、配置变化、关闭或新增重复绑定时取消未提交
部分；Esc 仍取消。普通临时队形 Hold 不会挡住同键盘 chord 的显式 MO
鼠标操作；已开始拖拽或锁定的队形模式保留其输入所有权。

Host 既有 polled PointerPress 的左按下 operation=7 被统一映射到物理
button=6 后预留；左右键释放仍按其捕获时原始 keys 检查。采样、旧鼠标
快捷键批次和 Runtime MSG 不会同时构造两条命令。预览的操作提示在
`Drawing` 捕获策略生成的字符串，绘制线程不读取可变键位设置。

自定义巡逻不要求实按 Z，也不合成 Z：规划事件的 append 字节与普通
PlanConnect/PlanCommit 自成完整流程。若用户同时处于游戏原生规划编辑，
通过既有只读 `nativekeys::load()` 找到真正 PlanningMode 的主键/修饰键，
延迟其原始释放消息，图闭环后重放由原生 UI 提交；此路不再另发一个
House 级 PlanCommit。未知编辑绑定拒绝开始，跨 match/House/window 的
旧释放丢弃。具体接口和证据见 `2026-11-02-patrol-key-coordination.md`。

本轮新增输入策略本身不安装 Hook、不写代码段、不写单位任务或原生规划
图。最终动作仍由既有 GameRuntime 原生事件网关及帧仲裁提交；是否启用
原生缩放/侧栏/暂停等另由整合层的不改代码段能力门决定。

验证：既有 `middle_input_owner.cpp` 补充改键/改鼠标、禁用、重复配置、
无效模式及 Double-Hold；`formation_quick_bridge_test.py` 直接执行生产
bridge，补充 G+侧键1、G+右键、其他鼠标取消及捕获中重绑不提交；
`mo_patrol_planning.cpp` 直接执行协调头并解析合成的真实 CommandClass
布局，检查无 Z/P 的自定义巡逻、实际 Ctrl+P 释放、重按、未知编辑状态、
跨 match 丢弃和 PostMessage 重试。均通过。x86 Windows 生产 bridge
交叉编译通过；整合层负责冻结后完整 x86/x64 构建。上述不代表实际
Windows 视觉、原生寻路或双机联机验收。
