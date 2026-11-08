# 统一世界采样契约：每逻辑帧一份原始样本，全部功能共用

## 结论与本轮状态

用户要求的是：**每个游戏逻辑帧集中获取一份实际的原始世界样本，自动功能、战术信息、主屏、副相机与小地图共同消费它；同一帧不再各自读取同一批对象和字段。** 这比统一唤醒、统一帧号、统一发布 epoch 或各功能自己的缓存更强。

固定源码基线为 `cee2af5621c78d5f565463cf611ff591ed1ecbc3`，版本头为 **1.0.44-pre.1**，`assist::Version=100`。该基线已有共同游戏线程发布 epoch、来源追踪、身份索引和局部去重；**尚不存在供所有功能与 Host renderer 共用的完整原始读集。** 本轮只核实源码并记录待实现契约，没有新增统一采样实现、测试、编译验收或版本发布。

此前 [硬件控制与工具箱计算热路径记录](2026-11-08-hardware-control-and-host-compute-hotpaths.md) 所说的「统一游戏线程采样」指已有发布组织方式，不代表多个 collector 共享了实际读取的对象字段。pre.1 的同帧 Scatter 门控和一次 combat collector 内的 fresh selector 批处理只覆盖其指定范围，不能延伸表述为全局统一。

已按根 `AGENTS.md` 先读 research INDEX、关联笔记及同一持久 Notebook v158。主要历史依据为 [Hotfix25](2026-10-31-coherent-compute-frame-hotfix25.md)、[独立场景读取](2026-10-29-independent-scene-reading.md)、[共享场景与本地 UI](2026-10-30-shared-scenes-local-ui-hotfix13.md)、[小地图完整战术范围](2026-11-04-minimap-camera-overview.md) 和 [深层同步边界](2026-11-01-deep-desync-followup.md)。两份只读审计摘要、审计文件 SHA-256 和关键固定源码 Git blob/内容 SHA 见[相邻证据 JSON](2026-11-08-unified-world-sampling-contract.evidence.json)。

## 一、现有 epoch 没有承载共享原始样本

`bridge.cpp::withInputPublication` 使用 `computeFrameSeq/Serial/Match/Tick/Frame` 组织多个功能的发布，Host worker 消费各自的输入包；`indexWorld` 建立 ID→pointer 索引。它们没有保存一份可让各 collector 复用的 XYZ、owner、生命、武器、乘员、地图等原始字节。

当前发布区域同时包含纯输入读取、任务维护、原生合法性查询、命令消费和事件提交。把整个区域套进缓存会跨越原生调用/写入边界；后采字段可能来自执行之后，却被标成同一个样本。必须先划分纯 capture、投影、查询与执行阶段。

### 仍存在的战术读取入口

| 原生成员表 | 已核实的独立入口 | 共享要求与边界 |
| --- | --- | --- |
| Techno `0xA8EC78` | `indexWorld`；planner own/enemy 两轮；dodge source/threat；missile source/target；autoload；cooperative；unload；internal scan | 一次获取完整指针序列并保留原顺序；不同功能上限/排序不能截断公共 inventory；同指针字段经同一原始读集。 |
| Building `0xA8EB40` | repair；AntiTheft；ore refinery 搜索 | 保留其独立成员关系，不未经证明等同于 Techno 建筑子集；与 Techno 重合对象的字段去重。 |
| Bullet `0xA8ED40` | dodge threats、visual projectile 相关需求 | 动态事实与公共对象引用统一；弹体语义不冒充 Techno 语义。 |
| Unit `0x8B4108` | ore inventory、miner defense 退役观察 | 缺席证明依赖完整且有效的 roster，不能来自 capped Planner World；即时执行守卫仍 live。 |
| Selected `0xA8ECB8` | 选中 stamp、autoload、命令、internal scan | 属于 UI/input generation；暂停或同帧选择变化不能被世界帧冻结，已有任务 cohort 也不能被后来选区替换。 |
| Type、weapon、passenger/radio/occupant、Cell | planner、combat、repair/load、cooperative、ore、renderer | 由同一 producer 按需求展开指针链和地图区域，保留上限、循环、失败与完整性语义，不要求整张地图和全部素材每帧重读。 |

确定的剩余重复包括：planner 的 own/enemy 两轮在判 pass 前已重复读 kind/live/HP/owner；missile 的 source×target 查询不自动获得 combat probe collector 的局部批缓存；cooperative 仍有自己的 combat 解码；miner defense 即使复用 planningWorld 列表，也重新读取身份、type/radio/dock/state。`oreHarvestObserve` 在 cadence 门前，其观察帧门控不等于 Unit 成员表已共享。AntiTheft 预扫描仍保留。

这些结论来自固定源码调用链，不是新运行的次数或耗时测量。

## 二、显示链还有多套真实采集者

`SharedProcessReader` 只是独立 `ReadSession` 的兼容别名；多个 session 共享只读进程连接，不共享当前页样本。现役生产路径如下，频率仅指源码调度目标或准入条件：

| 消费功能 | 当前采集所有权 | 仍存在的重复 |
| --- | --- | --- |
| 战术标签、敌方标记、状态 | GUI 的 `externalReader + TacticalSampler` | 新帧/相关变化可全扫 Techno；同帧刷新仍读已知对象的身份、XYZ、状态和乘员。 |
| 主屏地形 | 独立 `terrainWorker/terrainReader/terrainSampler` | 最多 60Hz、有界 Cell sweep，没有按逻辑帧唯一采集；完整 terrain 可能跨帧构建。 |
| 主屏 actor/FX | 独立 `samplingWorker/sampleReader/sampler` | observation 目标 1000Hz；同帧只在上次观察后 50ms 内早拒绝，之后仍可重新 snapshot。 |
| 小地图 | 一个 worker、自有 reader/Sampler | bank 不合格时另做 terrain step/snapshot；真实模式每个显示循环可重采，同帧也会进入。主副显示器共用该结果。 |
| 副相机 | 一个 watch worker、自有 reader/Sampler | bank miss 时自行采集；命中后仍可能扫描自动危险目标，并读跟随锚点、乘员等。一次准备服务最多三视角，并非每窗一个 sampler。 |
| 侧栏矿车/电力 | 独立 sidebar worker | 新帧、相关变化或 100ms 到期可重新遍历 Techno/矿车/矿场。 |
| 本地 UI、队形、建造、命令光标提示 | 各自 reader 或主 externalReader | UI 可重读 scene objects 的字段；Enter 光标提示可另扫 Techno；资产读取与即时许可检查也仍存在。 |

`watch_engine.h` 在该基线仅由测试包含；生产链是 `main.cpp → watch_host.h → watch_scene.h`，不额外计作现役 producer。

### SceneBank 的投影限制

`scene_bank.h` 只有一个进程内 mutex、身份/模式标记与最近的 `shared_ptr<const radar::Frame>`。它没有采样 owner、需求合并或同帧 in-flight 归属。多个消费者同时 miss 会分别采样；主屏关闭后，watch/minimap 也必须依靠 fallback。

而 `radar::Frame` 已经带投影：`radar_map.h::snapshot` 会按 legal cells、可见性和归一化 u/v 范围排除对象。小地图要求 Tactical overview，主屏来源保留 native projection，`minimap_overlay.h` 因不兼容而拒绝共享，并禁止把 adjusted overview bank 发布回原 native bank。

历史小地图笔记中的正常 64×64、live bounds `{2,2,60,56}` 例子，Radar extent 为 `[-1770,1830]×[1035,2745]`，Tactical extent 为 `[-1800,1800]×[945,2760]`。这是继承的受控证据，说明差异并非偶发失效。仅修改已过滤 Frame 的 Geometry，不能恢复被丢弃的边缘对象。

统一 provider 应共享**原始地图/Cells 与未按单一视图截断的对象事实**，由 native Radar、overview、主屏和 watch 分别投影。现有 SceneBank 可作为演进后的发布出口，但只给它加锁不够。

### 输出复用不是采样复用

`SceneRenderer::render` 和 `worldscene::Pipeline::draw` 仍读取当前光照、Cell shade、调色板/资产身份、shroud/fog、chrono 和径向指示器。`FrameReuse` 在这些 state/light/palette/mask 评估之后才比较输出输入，所以它不证明前面的 RPM 已消失。

watch 的相机锚点与 actor pose 必须取自同一世界样本。当前 `currentAnchors` 会更新私有画面中的跟随目标，避免相机追新坐标而对象/阴影仍在旧位置；迁移时需要同源替代，不能只删重读而保留较新的相机中心。

## 三、待实现的数据与阶段契约

### 三种更新域

| 数据域 | 目标更新规则 | 消费方式 |
| --- | --- | --- |
| 静态素材/目录 | 按资源、地图或配置代际及实际变更缓存 | SHP/VXL/地形素材、解码/GPU 资源不因显示器高刷或每个逻辑帧重复加载。动态字段不能伪装成静态缓存。 |
| 动态世界事实 | 活动功能与在途任务需求求并集；每逻辑帧一份共同原始样本 | 对象、弹体、地图动态状态等由唯一 producer 获取；自动功能和全部视图消费同一 sealed payload。 |
| 小型 UI/input | 独立的快路径与 input/view generation | OS 鼠标、窗口/DPI、视角、选择和光标交互可以同帧更新；只重投影/绘制，不触发另一轮世界全表采样。 |

这里的「每帧一次」是一个逻辑采集批次，内部允许必要的批量/分段读取；不表示一次系统调用读取整个游戏内存，也不要求把全图静态素材每帧重新获取。

### CaptureReadSet → seal → FrameRead

建议以共享原始区间读集和原生成员表为基础，复用现有 decoder，而不是另造一个被所有功能强行共用的大 Actor：

```cpp
CaptureReadSet::read(address, out, length); // 唯一 producer，获取缺失区间
FrameRead::read(address, out, length);      // sealed，只消费已有字节/失败记录
LiveRead::read(address, out, length);       // 明确的即时守卫、原生查询或执行
```

这是设计能力划分，**上述公共接口尚未实现**。

1. 采集入口固定 process/connection/world/House/map generation、match、logic frame、需求 revision 和 acquisition tick。基础原生成员表一次获取，保存 complete/failed/truncated 状态及原始顺序；公共身份索引从该表生成。
2. 已启用功能、在途任务、主屏/minimap/watch/UI 的纯输入需求求并集。capture builder 可在同一读集中发现 type、链和地图扩展；此阶段不能原生调用、发事件或写游戏。
3. 同一有效字节区间只采一次，处理整块与字段的重叠读取。只缓存 `(address,size)` 会漏掉重叠；每字节 hash 又会制造热点，具体分块/区间实现与预算仍待确定。失败需显式表示，不能把 fallback 零伪装成空表或真实字段。
4. 外层 frame/world/House 的采集前后校验使用独立 LiveRead。不能让缓存保证「必然相等」。校验失败就不发布混合样本；大地图有界 sweep 的静态来源与动态帧版本须分开表达。
5. 完成后 seal。全部 Runtime/Host 投影消费同一 FrameRead，缺字段/页不得悄悄退回游戏内存。已覆盖数据可以因配置或视图改变重新投影；新需求未覆盖的世界字段等下一逻辑帧，不生成隐形同帧副本。
6. 原生查询与执行发生后，sealed frame 仍是本次 acquisition 的历史事实，不能继续补写成为「最新」。下一逻辑帧再采。世界退役、进程/House 切换、rewind、地址复用与地图身份变化使旧 frame 失效，不通过更新 tick 续命。

中央 service 应独立于主屏开关；只开小地图或 watch 时也由同一 owner 生产。全局数组 inventory 和公共派生结果也应复用，否则共享页只减少物理读，各 consumer 的重复 CPU 遍历依然存在。纯本地按不同规则投影是允许的，不应与重复采集混淆。

### 跨进程必须共享真实 payload

当前协议只有 feature-specific inputs 和 `computeFrame*` 来源字段，没有 renderer 可消费的公共原始字节集。64 位 Host 与 32 位 Runtime 共用原始样本，需要版本化 mapping/payload，而非相同 frame stamp 后各读各的。

具体容量、buffer 生命周期和 provider 所在线程尚待实现设计；至少要有明确宽度的 address key、offset/length 边界、world/frame/serial/demand/coverage、失败/完整性标记和稳定发布/固定引用生命周期。如改变 Shared，须配对变更协议；新 mapping 也需要握手与生命周期版本。本轮没有调整 Assist100 或配对二进制。

## 四、不能被缓存抹掉的语义

`GameView::combat` 与 `rawCombatInfo` 并不等价。共用原始字节可以保留两者各自的 deployed/slot/rearm/lift/burst/兼容规则，不能顺手合并算法。`rawWeaponInfo(cached=true)` 既有的 30 帧派生元数据缓存也不同于 fresh selector；当前原始样本不得被旧 World Actor 替代，metadata 时效若要改变须另作有意变更。

当前 `readFresh` 同时承担普通动态字段和独立复核，需要明确拆分用途：

| 必须保留的边界 | 迁移要求 |
| --- | --- |
| 身份/归属/手动版本/lease/任务 cohort | 输入从共享样本解码；提交、撤回、立即停用仍验证当前事实，不能全局替换 `worldByID/rawObjectID`。 |
| 两次独立一致性检查 | watch 死亡继承、Cell/乘员链、输出主视角前后确认不能变成同一缓存字节自比。 |
| native combat、Enter、Unload、reverse 地形合法性 | 原生查询单独阶段，保留故障/零结果与即时合法性；不能跨 native call 扩充同一 raw capture。 |
| 名称含 raw 但实际有原生调用的路径 | 如 magnetron map height/cliff/air 查询，以及 internal scan 的投影/乘员/超武查询，仍须拆阶段，不能凭函数名判断为纯读。 |
| 提交时全表容量/预留/退役证明 | 共享输入 aggregate 不自动证明提交瞬间有效；尚不能证明等价的 live admission 必须显式保留，不夸称所有数组扫描已消失。 |
| hardware control | 即时 stop/fault、world/House、原属方、DR 所有权与恢复不改成同帧成功缓存。世界样本统一也不会消除公共控制入口每次 DR 异常的固有成本。 |

`consumeSubmittedIntents` 可能改变任务、选择、玩家或游戏。可将无游戏副作用的 mailbox 解析/局部 metadata 接纳置于 capture 前，再在 seal 后执行；或者完整执行阶段在前、随后采集。但不得采一半、执行，再继续补另一半。ACK/receipt、来源 cohort 和时效语义不能因拆阶段而丢失。

## 五、验收条件与未决事项

本记录不是实现完成或性能验收。后续至少需要以下源绑定检查：

- planner、missile、dodge、load/repair/cooperative 与 minimap/另一视图同时启用时，中央基础成员表一次获取，重叠有效原始区间只由 producer 获取一次；分别记录底层读次数/字节、capture 次数与全表遍历次数。
- sealed projection 使用任何 live read 都会失败的 reader；Host 缺字段不得回落 ProcessReader。计数不能只观察某一个 collector 的缓存。
- 同帧多消息/多视图、下一帧变化、新需求、暂停 UI 输入、地图边界变化、world retire/rewind、地址复用、失败/截断列表、重叠读取均保持有效性和失败语义。
- main 关闭后 minimap/watch 仍能获取同一源；native/overview 边缘对象完整；窗口/鼠标/视角变化只重投影；watch 目标与锚点同源。
- native/event spy 证明 pure capture 无游戏调用/写入，seal 后不补采；实际提交与撤回的身份/权限/手动令牌/原生合法性检查仍执行。
- 协议容量/跨位 layout、mapping 边界与发布引用生命周期有实际检查。输出一致 fixture 和操作计数不能替代 Windows、GPU、游戏 FPS 或联机验证。

外部 1000Hz 轮询不能证明捕获每一个游戏逻辑帧，frame 标签也不证明跨多次读的瞬时一致性。严格每帧覆盖、中央 provider 的线程/IPC 方案、全体 visual 扩展覆盖及预算、暂停世界与 UI generation、MOD 原生查询副作用仍需在实现中解决。

拒绝作为完成依据的替代做法包括：只统一 clock/epoch；多 Reader 共用连接；只套 exact-request cache；只统一三套 renderer 而漏掉战术 collector；把现有已裁剪 SceneBank 当全世界原始样本；把所有 `fresh` 全局改成 cache；用局部夹具次数推算实际 FPS。

## 证据来源与继承样本

本轮证据是固定源码与 Notebook v158 的只读核查。关键入口包括 `bridge.cpp`、`protocol.h`、`external_orders_bridge.h`、`dodge_bridge.h`、`missile_bridge.h`、`raw_combat.h`、`cooperative_reader.h`、`ore_harvest_bridge.h`，以及 `tactical_sampler.h`、`scene_bank.h`、`radar_map.h`、`radar_scene.h`、`world_scene_pipeline.h`、主屏/minimap/watch Host。准确内容摘要和两份独立审计摘要见相邻 JSON，不附私有游戏字节或原始 dump。

继承的支持样本 SHA-256：

- `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`：现有 raw combat、world/House、活动 Radar 等研究的主要依据。
- `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`：共享场景、本地 UI 的既有双样本研究。
- `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`：小地图/Tactical 原边界研究的历史样本。

上述地址与字段来自所链接旧研究及固定源码；本轮没有重新读取私人 EXE、反汇编、原生执行或新增地址/ABI。历史 controlled/native/game 记录保留原证据层级，不改称本轮实测。两份审计工作文件位于 `shared_frame/render-audit/visual-sampling-audit.md` 与 `shared_frame/tactical-audit/TACTICAL_SHARED_FRAME_AUDIT.md`；其 SHA 在相邻 JSON 冻结，持久结论以本笔记为准。
