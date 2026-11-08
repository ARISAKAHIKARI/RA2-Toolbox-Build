# 模块性能与正确性审查：重复扫描、订阅退订、渲染样本身份与完整分组保存

日期：2026-11-08（Asia/Shanghai）。基线：`a4450ee784ad7844cacf848f1319c55e10aa8c23`，`1.0.44-pre.4`。本轮修复版本：`1.0.44-pre.5`。

## 问题、范围与证据边界

用户要求逐模块、逐功能点审视重大性能问题及其他 bug。本轮按高频入口、采样/计算/提交、对象身份、缓存失效、线程与队列、开关/换局/重连、保存/退出几条链路并行审查，并交叉复核改动。确认五类基线缺陷，均有生产路径及受控执行证据并已修复。其余已审路径未另行确认重大缺陷；这不意味着全部功能已经通过真实游戏验收。

本轮没有读取新的私有游戏 EXE 或 dump；沿用既有受支持样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。先查阅本索引、共享采样/硬件控制/设置恢复研究与同一 SQLite Notebook；不重复认领已有修复。没有新增游戏地址、调用约定或内存布局。实际改动使用既有 `World Key/Token`、弹体指针与 UniqueID、原生调色行和完整 INI 分组语义。Assist 协议仍为 101。

专项验证在 Linux 上执行生产函数或生产类，替换边界限于合成内存、Win32 I/O 和原生命令入口。Windows x64 设置 fixture 仅交叉编译和链接。最终产品在固定提交和空输出目录完成 Windows 目标构建、实际 PE/配套文件检查，结果由交付包 manifest/provenance 绑定。本报告在源码冻结前编写，最终提交和二进制散列以随包证据为准。

## 五类已确认缺陷

### 1. 反向躲弹已处理弹体仍反复扫描全体单位

**影响：** 开启反向躲弹、存在以地格为目标的弹体时，高频唤醒会对已经处理过的弹体重复做完整目标收集。基线先执行 `collectReverseTargets`，然后才查 `ReverseShot::handled`；地格分支需要遍历 Techno roster。这个次序使已完成工作继续随“弹体数 × 单位数 × 唤醒次数”增长。

**修复：** 在 `reverse_dodge_bridge.h` 复用原槽位探测规则，先只读核对精确弹体指针、UniqueID 和 handled，再决定是否收集。空槽/过期槽提前停止规则保持不变。未命中不会创建状态、启动八帧重试计时或授权原生命令；持续中的既有任务仍然维护。没有对整帧一刀切节流。

**实际对照：** 64 个地格目标弹体、512 个单位、同帧 100 次唤醒。首次候选遍历均为 32,768 次；后续 99 次从 **3,244,032 次降为 0**；受控读取接口调用从 6,576,966 次降为 198 次。原生 actor 边界尝试均为 64 次，持续任务维护均为 100 次。18 场景、257 条检查，两版语义摘要均为 `2955511093`。

覆盖直接/地格目标、失败重试、八帧期限、缺样本、指针/UID 复用、旧采样身份、哈希冲突/满表、换局/重启功能及非弹体工作。读取接口次数不能当成跨进程 RPM 次数或实机 FPS。

### 2. 已停止使用的共享页请求残留，超时消费者仍触发额外采集与发布

**影响：** 基线只在新的非空页请求到来时清理页龄。关闭可视功能后，仍活跃的 Base-only 消费者可以续心跳，却不清理旧可视页。另一个问题是 Server 的 `wanted()` 判超时，但 `requests()` 和 `publish()` 继续使用旧需求；其他本地自动功能继续采样时，就会替已失联的 Host 读旧页和复制数据。相同页集合续订也不断重写列表和版本。

**修复：** 普通 acquire/subscribe/heartbeat 同时清理 250 ms 页租期；成员集合变化才发布需求版本，保留 2 ms 合并间隔。Host 心跳超过 1,000 ms 后，Server 清掉本地请求并退休活动发布。同连接新 Client 清理前任需求。`acquire()` 本身会续心跳，不能把 demand mask=0 等同于没有活跃消费者。

**实际对照：** 同一 fixture、每版 368 条检查。保持 Base 消费者时，残留 256 页和额外 256 次采集读取均变为 0，正常 Base 的 28 次读取保持不变。心跳超时后残留页 256→0，无活跃 Host 时 payload 分配 1→0。相同成员集合的 50 次重复发布变为 0；有效新请求仍保留。

**审查中修正：** 退休发布后，暂停帧原有 Runtime 快路径会跳过重新发布。最终实现可重新发布已有封闭样本，精确 Key、页数和 DataRef 去重保证重复唤醒不重复导出；恢复不产生 live 读取、不改 serial/tick，不修改旧 Token。该问题是对本轮修改的交叉审查发现，不另算第六个基线 bug。

### 3. 同一不可变世界样本被高刷新呈现反复完整验证地形

**影响：** 主画面 `prepareWorld`、实景小地图 `render` 和副相机采用同一 `SceneRenderer` 实现，各渲染 owner 持有各自实例。基线每次 presentation 都推进 evaluation epoch，并重新遍历 Cell 验证光照/几何；reader 仍绑定同一个封闭 Token 时没有新数据可以发现，重复工作依旧发生。

**修复：** 记录已经完整验证的弱 Token 身份及 crop/terrain owner，仅在实际 reader 与 Frame 使用同一 Token、缓存完整且条件未变时复用。资源、调色、精度、光照、几何和 reveal 失效检查仍在前面。新 Token 即使 game frame 相同也重新验证。弱引用不延长大块 raw payload 寿命，物体/特效/遮罩及最终 live 提交检查继续执行。

**实际对照：** 生产 renderer、10,000 Cell、600×300 图像、同 Token 连续 8 次呈现。每次光照求值 **10,000→0**，读取接口调用 **40,022→10**。跨版本完整输出 FNV64 均为 `1e04c69969630335`；各次运行内部的重复像素 vector 逐项相等。没有序列化跨进程 RGBA 做直接逐字节比较，因此不把哈希相同写成跨版本完整逐字节证明。

`prepareWorld` 正常 4,096-cell 分块预热完成后，相同 Token/crop 的 Cell 光照与几何检查次数均为 0；crop 改变后恢复几何扫描。最终 source-bound 微基准均值 3.052180→0.009003 ms，受并行构建负载影响，主要依据为工作量和正确性检查，不能换算为整场游戏帧率提升。

### 4. 调色转换缓存把同一 game frame 的不同 publication 当作同一份数据

**影响：** 基线 converter metadata cache 依据 scenario、game frame 和行地址，遗漏实际 raw Token 身份。同帧新样本中行内容或缺页状态变化时，可返回旧颜色表并错误维持 complete 状态。

**实际复现：** game frame=1、scenario/converter/row 地址不变，新 Token serial 从 1 变 2；行索引 1 从 `ffff` 改为 `07e0`。基线仍返回 `ffff` 且 complete=true，修复后返回 `07e0`。新样本缺少行页时表为空、reader incomplete，连续两次也不会缓存为成功。

**修复：** `scene_lighting.h` 显式区分封闭世界读取与 live/resource 读取。三个生产世界适配器显式 opt in，并须持有完整封闭 Token 才能进入新增的 sealed-source 缓存路径；旧/纯内存 reader 保留原有语义。弱引用核对准确对象身份。新 Token 重读真实行数据，内容相同保留已有 table 指针。仅仅持有 reader 指针不能授权缓存，live ResourceRead 不加入该路径。

新 serial、缺页/恢复、match、terrain owner+高度、资源版本、palette、precision、crop、generic live callback 已执行检查。reveal 切换、具体 Win32 SharedReadView 无绑定 Token、其他单独世界字段变化属于源码审查，未伪装成新增运行用例。原有 `scene_lighting.cpp` after 回归通过。基线 `--baseline` 在复现旧调色错误后返回，不执行修复版才有的失效断言。

### 5. 完整 INI 分组保存绕过异步失败、重试和关闭门控

**影响：** Host 只代理单键 `WritePrivateProfileStringW`，整节 `WritePrivateProfileSectionW` 仍在 owner 线程同步写。TypeHotkeys、AutoCSUnits、AircraftQuotaOverrides 每次保存都会整节写，TypeCatalogue 在目录更新时写。整节失败而普通键成功时，统一 Store 看不到失败，重试没有完整节内容可重放，关闭门控可能继续通过，导致设置丢失；同步磁盘调用也会阻塞 owner 线程。

**修复：** 新增内有完整 MULTI_SZ 的 ReplaceSection 操作，由同一异步 writer 调用原生节 API，纳入 desired-state cache、失败提示、pending、重试和关闭门控。失败节重试保留后续成功单键的补偿，涵盖 cache capacity 拒绝。仅对同一节维持排序屏障，无关节允许合并，以免恢复旧的队列溢出问题。一个 Host alias 覆盖上述四条入口。

**实际对照：** 从 Assist 提取真实 `writeSection()` 和 dispatch alias，与真实 Store 连接，原生边界仅注入首次写失败。owner 线程整节 API 调用 1→0；失败从不可见变为可见；失败后的关闭存储谓词从可通过变为不可通过；显式重试从无法保存变为成功。

完整 portable Store 回归覆盖 MULTI_SZ 所有权、Unicode/空值/含等号值、旧键删除、同节顺序、失败及容量拒绝后的补偿。25 次保存 ×（200 普通键+3 整节）合并为 203 个 pending 操作。Windows x64 fixture 已编译链接，未在 Windows 上执行。未测真实磁盘延迟，未驱动完整 Win32 WM_CLOSE 序列，也未把多节保存声称为单个事务或断电持久性保证。

## 功能覆盖清单

下表为入口及关键生命周期源码审查，只有明确标明的修复有专项执行证据。各 reviewer 的详细功能表、命令、散列与原始日志保存在 Verification 包。

| 模块/功能点 | 本轮审查结论与边界 |
| --- | --- |
| Runtime dispatcher、2 ms/帧节拍、Unknown/ViewOnly/换局/重连、清理恢复 | 检查工作准入、世界身份和清理顺序；共享订阅修复见第 2 类 |
| 反向躲弹、普通散射躲弹、重试和持续任务 | 反向模式修复第 1 类；普通模式未另行确认缺陷 |
| 导弹拦截、手动接管、held-unit 释放 | 核对预算、快照/原生查询、结果准入；既有大栈修复保持 |
| 采矿回矿、卸矿后续采集、矿车自卫 | 核对帧/尝试门控、增量地图预算、手动状态和目标身份；未另行确认缺陷 |
| Planner/防偷、自动装载、载具候选和容量 | 核对 Host/Runtime generation、保留/重试和原生合法性；防偷预扫描为已有行为 |
| 智能卸载/弹出、乘客历史与后续任务 | 正确性入口未另行确认缺陷；乘客历史重复扫描列为待测候选 |
| 磁电/高级攻击、目标与落点合法性 | 核对帧重建、手动暂停和最终准入；永久索引饱和假设被每帧重建证据否定 |
| 队形、MO 协作/巡逻、选择模式、Sticky | 核对显式命令队列、布局节拍、身份与手动取消；大选择操作仍缺实机量化 |
| SP/MP event gateway、网络发送保存 | 核对容量、actor lease、OutList/DoList、外来命令顺序、世界/故障退休；无新增确认缺陷，未跑联机 |
| 玩家切换、Destruct 显式动作、House/sidebar 恢复 | 检查权限、选择身份和上下文恢复；未另行确认缺陷 |
| 硬件断点服务、科技/控制全部、地图/FPS 分流 | 检查四槽策略、owner、清理、TF/RF、候选表、原生 true/strict 分流；保留 pre.3/pre.4 改动，本轮不重认领旧成果 |
| 放置、超级武器、CRC/特性 hook 恢复 | 检查 native 边界与关闭恢复；未另行确认缺陷；已退休 burst 功能不当成当前活跃缺陷 |
| 共享世界 Client/Server、页需求、payload 与 Token | 第 2 类修复；368 条专测及完整 core 回归 |
| 外部采样、进程身份、缺页、跨消费者样本 | 检查 sealed/no-live-fallback、连接及 producer generation、暂停/倒退；未另行确认缺陷 |
| 主画面、实景/示意小地图、三个副相机/跟随 | 第 3、4 类修复；高刷真实视觉/GPU 尚需 Windows 实机 |
| SharedSceneProvider/SceneBank、WorldScene Pipeline | 检查同 Token 去重、crop/epoch、遮罩、FrameJob、immutable front 和提交门控 |
| terrain/native background、palette/VPL/assets/pose | 第 3、4 类修复；既有全图重建和 pose 峰值修复不再计为新问题 |
| Latest/Recurring worker、三槽 frame pool、overlay compositor | 检查单 pending、锁外工作、版本复核、750 ms 过期与层提交；未另行确认缺陷 |
| GPU 生命周期、fence、fallback | 生产等待上限与测试等待区分；已有 DirectPresentation 关闭决策保持，未做驱动故障实机验收 |
| 光标坐标、DPI/全屏/负屏坐标、clip/hotspot | 检查当前坐标来源与像素边界；无新确认缺陷，多屏实机未运行 |
| 光标缓存、sample latch、focus reentry、动画取消、HCURSOR/GDI | 检查引用/销毁/缓存预算；不重复计入既有闪烁与复用修复 |
| Shell/页面启动、导航身份、tab、错误/重连面板 | 核对 worker 启动与发布；无新确认缺陷 |
| 浏览器动作路由、receipt、动态选项、snapshot | 核对 256 动作/owner、512 receipt、16 动作/turn、150 ms 新鲜度和 generation；无新确认无界积压 |
| 设置 debounce、整节写入、重试、PrepareClose | 第 5 类修复；统一存储谓词验证，完整 Windows 退出流程未运行 |
| Host 控制节拍、外部计算 worker、planner 结果 | 核对序列跳过、一次运行、参数/世界一致性及 late guard；无新增确认重复提交 |
| 自动附加、PID/创建身份、窗口重绑、broker | 核对进程保留和重连；broker 生命周期等待列为候选 |
| Unit monitor 采样、demo/reconnect、图标、可见性/退出 | 核对 250 ms 异步采样和 generation；已有独立 4 Hz 统计采样不算本轮回退 |
| GameRuntime 入口、frame signal/native tooltip | 核对 callback 工作边界、补丁所有权和恢复；无需求帧诊断检查列为候选 |
| hook_diagnostics、neural_renderer、runtime_broker 小模块 | 补查 entry/ABI、输入边界及生命周期；详细只读结论见补充报告，未新增确认重大 bug |

## 尚未确认严重程度、此次未改动的候选

1. **智能卸载乘客历史采集。** `observeUnloadRoster` 在每次获准 tick 扫 Infantry，并线性查历史乘客。直接加“同帧跳过”会影响同帧切换选择和 native D 先于 Host Ctrl+D 的历史桥接。需先采集真实大运输队规模下的耗时/触发频率，再决定按输入版本缓存还是调整历史结构。
2. **批量修理结果准入。** result×input 的嵌套匹配可能在大规模建筑选择时放大；尚无代表性场景测量，不凭 8,192 上限认定重大 bug。
3. **broker 生命周期等待。** 启动/交换等待 3,000/2,000 ms 时持有相关锁，可能增加附加/恢复延迟；未证明高频卡顿或死锁。应围绕重连阶段采样，不能直接改原有所有权时序。
4. **帧诊断未启用时的检查/探测。** `GameRuntimeHook` 每个 PM_REMOVE 调 `frameobserver::maintain`；没有诊断 channel 时，也先检查世界状态再尝试打开 channel。正常 playable 分支约六次自进程 RPM 是源码路径推导，非运行计数。Host 仅在明确诊断模式时创建 channel；发现新 channel 的探测仍有功能需要，可研究把状态验证后移或为探测加有界节拍。尚未量化实际消息量/CPU/FPS，不列为第六类已复现重大缺陷。

其他未采纳假设：当前默认反向躲弹保留 Runtime、21 条科技冷路径、超级武器 per-frame grant、防偷预扫描和独立 4 Hz 统计均有已有设计约束；GPU direct presentation 关闭是既有决定。没有用整帧跳过、live 回退读或强行修改联机归属来换取表面计数下降。

## 证据索引与后续验证

同目录 `.evidence.json` 记录最终生产/fixture 散列、对照收据身份、覆盖报告和明确边界。Verification 包包含每项命令、退出码、编译/运行日志、before/after 结果、只读审查、完整构建 provenance 与更新包成员散列。

专项执行包括 reverse dodge 对照、subscriptions 对照、world core（portable 无异常 74,634 / 模拟平台 75,357 checks）、terrain/palette 对照、已有 scene_lighting、完整 portable async Store、Assist 整节 dispatch 失败/重试；Windows Store 是 compile/link-only，不与已执行用例混为一谈。五类问题已在受控边界验证，仍需 Windows 游戏验收真实帧时间、大兵团、反复开关/换局、暂停恢复、磁盘失败/关闭行为、多屏/GPU和多人同步。没有提供未经实机测量的加速百分比或“全部功能零 bug”结论。
