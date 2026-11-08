# 共享世界采样实现：封口原始读集、跨进程发布与消费者阶段拆分

## 问题、基线与证据层级

本轮实现用户在[统一采样契约](2026-11-08-unified-world-sampling-contract.md)中要求的实际共享读集。
源码基线为 `c03e4ffcba13c6ceb435b758989d7c95093ca772`，其中生产代码仍与
`cee2af5621c78d5f565463cf611ff591ed1ecbc3` / 1.0.44-pre.1 一致。此前只有共同 epoch、
局部 combat 批复用及 Scatter 门控，不能称为全部功能共用原始数据。

按根 AGENTS.md 检索了 INDEX、关联研究和同一 Notebook v159。地址与字段继承现有生产
decoder，没有新增私人 EXE 读取、反汇编或原生 ABI 推断。沿用主要样本 SHA-256：
`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`；视觉研究的历史样本
`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` 和
`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6` 保留原证据范围。

本轮证据是源码、真实生产函数配合合成内存/Win32 边界的可执行夹具、Windows 目标交叉编译
与静态 ABI/PE 检查。没有 Windows 实机、GPU 画面、游戏 FPS 或联机验收。最终测试与构建
冻结内容分别由相邻 evidence JSON 和交付包的 build provenance / focused-validation 记录。

## 一、真正的 capture → seal → projection

新增 `shared/source/world_frame.h` 保存带身份的原始页与有效/失败区间。唯一 Runtime producer
在现有游戏线程调度准入后建立 capture。第一次访问先尝试整页，部分页失败时只尝试尚未观察
的所需区间；成功的重叠字节不会被第二个 collector 再次从游戏获取。原生成员表的指针顺序、
重复成员和独立成员关系保留，Building 不被替换成 Techno 的一个截断子集。

采集前后用独立 LiveRead 比较 scenario、Cells backing/count、session、House 和逻辑帧。
它们不是同一缓存字节的自比。变更、无效 roster、容量溢出或大页分配失败均拒绝发布。
封口后 `Snapshot::read` 只有查表与复制；Host 的 `read/readFresh` 在绑定 Token 后都走该路径。
缺页不会回退 RPM，也不会在同一逻辑帧补采，而是登记下一帧需求。

Runtime 与 Host 都持续续订成功读取的页，而不只登记失败页。否则一次补齐后的页会在下一帧
丢失、形成隔帧失败。请求有上限；大图/对象工作队列超限显式失败，不把截断列表当完整数据。
显式 optional 只用于原有明确降级语义的可选字段：已尝试但不可读时保留其默认策略；尚未
覆盖仍是 incomplete。必需字段失败不能悄悄变成 0、空表或死亡对象。

`CaptureGraph` 按 Base、Combat、Passengers、Ore、Bullets、Visual 的实际需求展开已有字段：

| 数据 | 继承来源/地址 | 展开规则 |
| --- | --- | --- |
| Techno / Building | `0xA8EC78` / `0xA8EB40` | 基础完整成员表，重合对象字段共享 |
| House | `0xA80228`，本地 House `0xA83D4C` | owner/country/factory 链，独立即时权限检查仍 live |
| Unit / Bullet | `0x8B4108` / `0xA8ED40` | Ore 或弹体/视觉需求触发 |
| 类型、武器、乘员、radio、temporal | 现有 raw_combat/GameView/visual decoder | 有界依赖展开，按需求避免无关武器/效果扫描 |
| Cells 指针 inventory | Map `0x87F7E8` 的既有 backing/count | Visual/Ore 才获取，具体 Cell 页由消费者需求驱动 |
| Ore 类型与地形速度 | `0xA83CE0`、`0xA83D80`、`0xB0F4E8`、`0x89EA40` | 延用现有 docks/tiberium/overlay 字段，不调用引擎 |
| 动画与原生视觉效果 | 已有 Anim/Terrain/Particle/VoxelAnim/Frozen/RadarEvent/Bolt decoder | 保留 type、attached target、建筑动画槽及记录数组依赖 |

Selected `0xA8ECB8` 属于 UI 输入域，不因世界样本冻结。同帧打开新功能可能需要下一帧才能
获得新增字段；没有用第二次世界采集掩盖这一边界。现有消息准入与采样频率保留，语义是
“每个实际观察到且获准处理的逻辑帧最多一份”，没有声称覆盖了所有模拟帧。

## 二、跨进程 payload 与内存预算

Assist 协议由 100 升为 101，新字段追加在原有布局之后；配对 Host/Runtime 使用新的 mapping
命名空间。`hardwareperf::Snapshot` 固定 96 字节；新的 world sample 指标通过独立 seqlock
发布。Windows x86/x64 verifier 同时检查 Shared、原始 Key/Page 和传输描述符的字段偏移。

原始 payload 版本为 1，传输版本为 2：`Local\\RA2WorldFrame_v002_<pid>_<owner>` 是
65,900 字节的小控制区。两个数据槽只存描述符，实际页数据在独立命名 mapping 中按需求取
2 的幂增长。数据头绑定 PID、Host owner、Runtime incarnation、连接 generation、槽号和
allocation ID。Host 只读映射，在复制前后检查 descriptor/active/sequence/connection，最终
产生自己持有的不可变 Token。已经固定的旧 Token 不会随双缓冲复用而改变。

| 合成样本页数 | 每槽容量 | 控制区＋双 payload 请求字节 |
| ---: | ---: | ---: |
| 0 | 0 | 65,900 |
| 28 | 32 | 394,220 |
| 512 | 512 | 5,317,100 |
| 513 | 1,024 | 10,568,172 |
| 4,096 | 4,096 | 42,074,604 |
| 16,384 | 16,384 | 168,100,332 |

以上是生产代码向 mocked Win32 请求的字节数，不能当作真实 Windows commit/工作集。
普通小需求不再固定申请约 160 MiB。容量稳定或缩小时不重复 Create/Open/Map，直到退役才
释放已获得的峰值容量。增长失败保留上一 active publication；打开/映射失败不回退实时读取。
增长期间旧 view 和替代 mapping 可短暂共存，真实 32 位地址碎片压力仍需实机确认。

Runtime/Host 的大 Page buffer 也改为从零开始、受限且 nothrow 的连续存储。失败保留原指针、
容量和已采页，标记 overflow；同帧不反复分配；reset 复用容量。通过可注入分配器在
`-fno-exceptions` 下执行拒绝增长测试，避免新增的大页 vector 在 Runtime 中因 bad_alloc
直接 terminate。其他有界小容器没有被宣称已经完成整个进程的 OOM 防护。

最终完整 DLL 的严格 PE 导出检查抓到了分配器链接边界：Zig 0.13 的 libc++ nothrow-new
库入口仍带 try/catch，调用方 `-fno-exceptions` 不足以阻止其引入 25 个额外 unwind 导出。
改用 `std::malloc/free`，并静态断言 `Page` 对齐不超过 `std::max_align_t`；placement new
和注入分配器失败测试保留。同工具链小 DLL 对照从 25 个额外导出降至 0，生产核心回归
重跑通过。另外三处局部静态工作数组的默认初始化会调用 libc++abi guard，再次引入相同
unwind 库；真实类型对照证明仅增加 `{}` 值初始化即可消去 guard，保留作用域、存储期与
初始字段值。`std::sort` 的独立对照没有引入该依赖，排序和线程安全初始化配置未被改动。
完整组件仍需通过原来不放宽的导出集合门禁，最终记录在 build provenance。

## 三、战术计算的读取、原生查询与执行分开

`SampleReadScope` 只允许封口读取；其内的 `put/call/callFast` 拒绝游戏写入和原生调用并
标记失败。平台 unwind 的外层 finally 恢复读域，嵌套 scope 不丢失外层失败。

- planner、cooperative、repair、dodge、reverse dodge、missile、autoload、unload、Ore 和
  miner-defense 的纯输入从共同样本构造；先写临时对象，完整后才替换发布输入和 serial。
- combat 的 raw 与 native 查询保持各自语义。纯 raw batch 不会因读取中配置变化而突然
  调原生函数；native 查询在退出读域后执行，随后的身份/手动版本验证仍 live。
- AutoLoad 保留原点击 cohort，不用后来选区替换；纯候选/容量、native Enter 矩阵、纯分配和
  实际提交分阶段执行。Unload 的原生部署/Enter 判断也退出纯读域。
- Ore 观察和搜索在失败时回滚 observer/search 状态；一次收集多项缺页需求，避免每帧只暖
  一个 Cell。实际资源再校验、完整 Unit roster 的退役证明及 native order guard 仍当前读取。
- 手动输入版本和新 intent serial 不由旧世界样本推进。Missile 原生查询期间新来的手动暂停
  不会被纯采样回滚清掉；planner 使用实际捕获的 inputSerial。
- Repair 输入失败保留旧 input、ID 队列及 FrameHistory；内部/Host 计算位置都按实际需求获得
  原始样本，不再让内部计算因缺少 Host 订阅而永久等待。缺样本/缺页返回 defer 时不消耗
  自动修理 interval，下一份完整帧可立即重试。

最终事件的身份、归属、cohort、manual token、lease、原生许可与容量校验没有替换成缓存成功。
必要的提交/撤回/退役扫描仍可能读取完整 roster；这不同于多个纯 collector 各自采同一世界。

## 四、显示与本地 UI

主屏、小地图和副相机共用 `SharedSceneProvider<ReadSession>`。同一原始 Token 最多一次
场景采集，失败尝试和迟到的效果需求也不触发同帧第二次扫描。主屏独立 terrain worker、
minimap fallback sampler、watch 私有 sampler 已移除；只开小地图或 watch 仍可成为消费者。

共同场景覆盖完整合法物理 Cell/对象范围，各视图再投影。native Radar backing 身份仍保留，
不会用已经裁剪的主视图伪装全地图。测试修复了 reveal 模式下北侧 Tactical 边缘被错误排除的
问题。watch 锚点、对象身体和姿态来自同一 Token。

暂停保活依赖独立 producer observation 确认当前世界和同一逻辑帧；不改 acquisition tick。
新帧、rewind、换 House/scenario/map、Runtime 重载或 PID/连接重用会拒绝旧身份。
完成画面的期限与原始采样时间分开，不能通过“画了一次”给旧世界数据续命。
本地 UI 异步结果展示也必须与目标场景的完整 Key/serial 一致，不能把旧样本文字拼入新场景。
战术标签在完整世界身份不符时清掉旧 publication；失败后的 camera-only 刷新恢复上一完整状态。

战术标签、侧栏统计、编队预览、命令光标、飞机路径观察、建造预览和本地 UI 也绑定共享原始
样本。飞机路径先纯观察，再保留三态 live 身份判断和下令前复核；未知读取不当作死亡并取消
任务。建造选择/绘制请求失败保留上一份稳定内容；最终 factory/capacity/退役资格仍 live。

选择、hover、鼠标、viewport、消息和用户规划拓扑是独立 UI 域，前后校验保持真实当前读取。
选择位从一次 live Selected 集合派生，不逐对象重新读取选中状态；UI 复用场景对象集合，
避免每次高刷重新枚举 Techno。actor 坐标、生命、owner、timer、目标和路径端点仍 sealed。
SHP/VXL/HVA/TMP、font/CSF 等静态资源走明确的资源 reader 和原有缓存；动态灯光、palette、
VPL、Cell 与对象状态没有被伪装成资源缓存。独立 unit_monitor 工具不在本轮统一消费者
接线范围内；没有声称整个工具进程的所有实时读取都被消除。

## 五、验证、拒绝的替代方案与未决项

专项检查包括真实 Snapshot 重叠字节去重、失败区间、完整成员表、需求续订、容量边界；
真实 Server/Client/ProcessReader 的双 publication、同槽重用交错、退役、连接重用、暂停、
只读映射、故障恢复；Runtime 采集期间独立改变 frame/scenario/cells/House 的拒绝路径；
以及各战术 collector 的字段/分配结果对照和 native/event 边界。

三视图并发、主屏关闭、地图边缘、watch 锚点、暂停原始时间、新需求、optional/required
失败和嵌套 scope 有执行检查。标签测试包含封口后世界变化、同帧选择/镜头变化、失败后
camera-only 路径保留完整发布和下一帧恢复。Host 路径/建造测试使用真实 ProcessReader 与
world-frame IPC，覆盖未知读取、旧 allocation、下令前 ID 变化及稳定内容保留。

这些检查按场景与源码 hash 记录，不把逐字节断言数或合成世界操作次数换算成 FPS。
明确拒绝：只有共同 frame tag；每个 reader 独立采集；exact-address/size 缓存漏重叠；
缺页 live fallback；把所有 fresh 改缓存；把 native 调用藏在 raw collector；刷新 tick 伪造
暂停新鲜度；只给 SceneBank 加锁而不共享 payload；为去重降低用户设置的刷新频率。

[硬件异常优化记录](2026-11-08-hardware-dispatch-and-rate-observation.md)覆盖本轮第二项性能
工作。外部异常先分流、维护状态复用和实际命中窗口已实现；每次 owned 硬件异常本身的 CPU/
Windows 投递成本仍在，没有新低频拦截点的证据。日志可定位实际异常率与原始采样量，
真实地图规模、32 位内存压力、GPU 视觉、FPS 改善幅度及联机仍待现场数据。
