# 显示链逐功能审查结果

## 结论与覆盖口径

本轮显示侧清单 **152 条全部完成所记录的源码路径追踪，剩余未追踪 0 条**。清单以当前 UI、设置规格、命令分发和实际实现为依据，没有使用仍停留在 1.0.2 的 `tools/build_feature_list.py` 作为全量目录。逐条入口、模式、实现路径、发现、证据和验证缺口见 `render-matrix.json`。

**源码审查完成，缺陷尚未全部关闭，也没有完成实机验证。** 当前仍有两个明确未修复发现：暂停帧空闲飞机按钮消失，以及侧栏满 75 项时建造时间提示被排除。另外保留一个未证实的副屏拖动边界竞态。

| 范围 | 清单条目 | 已追踪 | 未追踪 |
|---|---:|---:|---:|
| 共享显示设置与具体显示结果 | 68 | 68 | 0 |
| Cursor 设置、动作及原生/外置显示 | 40 | 40 | 0 |
| Unit Monitor 统计与展示 | 21 | 21 | 0 |
| 主画面、小地图、副相机及底层显示路径 | 16 | 16 | 0 |
| Neural renderer 保留 helper | 7 | 7 | 0 |
| **合计** | **152** | **152** | **0** |

152 条包含设置、动作和底层支撑路径，存在实现复用；不能称为 152 个互不重叠的独立功能。也不能与其它审查者的共享基础设施行直接相加成为独立功能总数。

矩阵状态为 `source_reviewed` 146 条、`fixed_controlled_verified` 4 条、`source_reviewed_open_finding` 2 条。4 条已修复验证行对应 Cursor 的两个缺陷共用一行，以及暂停标记问题关联的三行；不是新增四个互不相干的缺陷。

初始基线：`c3154fa343a5be42da3453c0b3d228dea653d3ac`。生产修复归入父任务提交 `38c6a71f840615986c6d37e1038034183c466566`，暂停标记修复后最终构建提交为 **`491bbc72ec67ac70454e9d9a0ef69287eb1ba28f`**。显示审查者在生产冻结后没有继续修改 `source/`。

## 已修复并实际执行受控验证的缺陷

### RENDER-F1：光标相邻帧形状校验的有符号溢出

生产路径是 `source/cursor/source/main.cpp::draw` 的未知动画连续帧推断，进入 `cursorstream::plausible`。原始资源解码允许最高 512×512 画布；旧校验把质心乘积和平方放进 `long long`。实际执行旧生产头文件，合法 128×128 不透明输入即触发 UBSan：`5905580032 * 5905580032 cannot be represented in type long long int`，进程退出 1。

修复在乘积与平方前提升到 `long double`，保留面积、距离阈值和原有判定行为。修复后执行真实生产头文件的夹具，覆盖 55/128/256/512 画布、近/远/空形状、空指针、非法 stride 和 Detector 的连续/重复/失效/复位分支。

影响范围需要限定：当前导入 UI 要求 55×43，默认资源没有被证明触发该溢出。大画布触发依赖可被解码器接受的原始缓存或旧采集路径。

证据：`cursor-shape-before.json`、`cursor-stream-after.json`、`cursor-shape-validation.json`；生产修改为 `source/cursor/source/stream_inference.h`。

### RENDER-F2：同一光标帧在高刷新循环重复扫描完整形状

原 `draw` 在未知动画表且已推断连续流时，每个显示 tick 都重新构造两个 canvas mask 并扫描，即使源 frame 没有变化。新 `PairShapeGate` 缓存当前相邻帧的接受或拒绝结果，frame 变化重新计算，原始 SHA 替换先复位。

受控执行的实际输出是 **10000 次相同帧求值只进行 1 次生产形状扫描**；新帧再计算，reset 后相同 frame 的不同资源重新检查，拒绝结果也缓存。这个结果证明该分支的重复扫描被消除，不能换算为整工具或游戏 FPS 提升。

证据：`cursor-stream-after.json`；生产修改为 `stream_inference.h` 和 Cursor 的 `main.cpp`。

### RENDER-F3：暂停时敌军、蜘蛛与箱子增益标记在 250ms 后消失

父任务确认 `updateEnemyDisplay/updateStatusDisplay` 接受已确认且同 match/frame 的样本，但 `paint` 的敌军和状态数量仍单独要求小于 250ms；同封闭 frame 的 `refreshCamera` 正确保留旧采样 tick，两个条件因此不一致。

修复前使用真实 `TacticalSampler`、Motion 和从生产 `main.cpp` 原文抽出的绘制计数语句复现：采样 tick 为 1000，1249 时数量为状态 1 / 敌军 4，1250 和 2000 时更新路径仍接受，但绘制数量变为 0 / 0。

父任务将更新与绘制统一到 `displaylease::frameCurrent`，保留原始采样 tick，并要求确认同 match/frame。修复前后实际执行 18 项检查，包含新 world/frame、失去确认、off/null、非法 count、零 tick 以及时钟回绕；修复后暂停同 frame 保留 1 / 4，失效条件仍拒绝。

证据：`render-root-supplement.json`、`overlay-paused-gates/baseline-regression/result.json`、`overlay-paused-gates/production-after/result.json`。修复归入最终提交 `491bbc7`。没有真实 Windows/GDI 或游戏画面执行。

## 已确认而本轮未修复

### RENDER-F4：暂停同 frame 后空闲飞机按钮超过 500ms 被隐藏

`source/assist/source/aircraft_indicator.h::updateAircraftButton` 仍要求 `sampleRenderClock - idleAircraftTick <= 500`。当 `sampleExternal` 对同一封闭 source token 走快速路径，只调用 `refreshCamera` 时，idle 采样 tick 不更新，按钮因年龄门被隐藏。

已实际编译并执行真实 `TacticalSampler` 夹具和从生产文件原文提取的**完整按钮隐藏条件**，固定同 frame，观察结果如下。

| 展示 tick | idle 样本 tick | 按钮被隐藏 |
|---:|---:|---:|
| 1000 | 1000 | 否 |
| 1500 | 1000 | 否 |
| 1501 | 1000 | 是 |
| 2000 | 1000 | 是 |

证据：`idle-aircraft-paused-gate/result.json`；编译、运行退出均为 0，记录生产文件 SHA256 和提取条件。前台、启用等外部边界由夹具控制，没有执行 Win32 绘制或真实游戏。

**本轮未修复。** 父任务进一步确认不能直接套用 F3 的同 frame 放行：idle 采样有 100ms 节流，整体 `state.frame` 可能先前进，而 idle 数组仍来自较旧样本；`invalidateSample` 也不会回滚 idle 字段。需要独立 idle 快照身份与有效性设计。点击路径仍有 500ms、`commandFresh` 和 Runtime 身份门，此处确认的是显示消失，没有证明会越权或向错误对象下令。

### R-TOOLTIP-075：侧栏满 75 项时不附加建造时间

硬件审查者追踪了 `BuildTimeTooltip` 从设置、Assist 需求、Runtime 共管文字槽到实际回调的路径。`build_time_tooltip.h` 在 `count >= 75` 时直接返回原文；同仓库 `sidebar.h` 的已有项读取/扫描接受 `count <= 75`，只有新增项的容量门才应排除已满状态。

因此，满 75 项栏目不附加建造时间是明确的源码边界行为。列为低优先级、**源码确认且运行未复现，未修复**。没有通过实际 callback 或游戏构造满栏目；既有独立 `cursor_tooltip_windows.cpp` 构建尝试因 `_RuntimeScopedCall` 链接失败，没有执行，不计为通过。

证据：`render-hardware-supplement.json` 的 `display.BuildTimeTooltip` 行、`tooltip-fixture-build-attempt.json`。独立夹具链接失败不代表最终生产集成构建失败，两者证据分开。

## 未证实的边界风险

### R-MONITOR-DRAG：新工作区与旧统计面板尺寸组合

Unit Monitor 的 `beginSecondaryDrag` 可以从 `secondaryMonitor.get` 取得更新后的工作区，并从当前 overlay 窗口取得尚未重排的旧面板尺寸。`secondaryDragSettings` 使用 `std::clamp(origin + delta, work.left, work.right - drag.width)` 等表达式，没有先保证上界不小于下界。

当游戏窗口跨到不同大小显示器时，Monitor 缓存可立即刷新；若拖动开始恰落在面板重新布局之前，理论上可能组合新小工作区与旧大面板，违反 `std::clamp` 的前置条件。`WM_DISPLAYCHANGE` 路径已有结束拖动/重置处理，但普通游戏窗口跨屏不一定经过它。

这是**已完成源码检查后保留的未证实竞态**。本轮未动态复现其具体事件顺序，不能称为已确认用户故障，也不能报该边界通过；没有修改源码。对应矩阵行 `monitor.secondary/secondary_x/secondary_y` 的 `review_risks`。其它多屏、DPI、热插拔和窗口跨屏运行同样没有实机验证。

## 实际可达范围与旧配置澄清

- **RenderUpgrade**：当前 UI 入口明确禁用，但已保存启用状态和子设置仍可进入保留运行路径；不能称为当前可新开启的主画面升级。
- **RenderPhysical**：当前 `effectiveRenderScale()` 固定为 1，`renderPath` 的局部计算结果没有参与实际输出尺寸或采样算法分支。物理几何探测是只读资格/诊断，不能将这个保留 flag 报成独立 4K 重建效果。
- **xBRZ**：当前有效路径为原生/最近邻。保留 CPU/GPU 后端只支持 2/3/4×，不是初始库存所写 1×–6×；旧配置的存在不证明当前 UI 已启用重建。
- **GPU**：`source/shared/source/gpu_surface.h::DirectPresentationEnabled=false`。GPU compute 仍可用，当前结果经 CPU 像素读回发布；共享图像直出/DirectComposition 分支属于保留源码。
- **高刷移动**：实际 `Timeline::sampleLatest` 根据最新两次观测做位置前推，上限为一观测间隔且不超过 25ms；不是仅在两个历史帧之间插值，也不能照头注释说“完全不预测”。它不推进游戏、SHP 帧或原生 timer，过旧/身份不连贯/瞬移等条件回退观测位置。
- **Cursor 五连点兼容设置**：clickBox/clickModeBox 当前创建时不带 `WS_VISIBLE`，gap 控件显式隐藏，并不加入高级控件列表。保留配置/兼容执行路径已审查，分类修正为保留配置。
- **Neural renderer**：当前 Assist/Suite 没有调用 `neural_renderer_client.h/neuralclient::`。七条协议、进程、像素/NGX生命周期路径为保留 helper 审查，不是当前用户可开启功能，也没有 NGX/GPU 运行证据。
- **本地 UI 层**：实际是消息/编辑文本，任务/超武/停电计时，主要工厂/电力/军衔/组号/维修/pips，集结/规划线，规划高亮五层。Runtime tooltip 和反馈软件光标分别有自己的路径，不能混作这五层。

这些修订已写入矩阵，避免以旧目录或保留代码夸大当前功能。

## 高频路径、缓存与生命周期审查结论

主画面、小地图和副相机使用同一共享 SceneBank 生产者，各视图的效果需求合并。源码身份包含 PID/match、source 连接与 serial/frame、house/cells、geometry/reveal；同源重复请求不反复采整世界，晚到的扩展需求等待下一发布。保留世界坐标与当前相机分离，完整 world 持有素材所有权；实例复用按 shared owner，完成帧复用按全部有序绘制、材质、深度、遮罩和效果输入，不能仅凭游戏 frame 相同复用。

地形缓存检查了 theater/资源/palette/geometry 失效、按 presentation epoch 的重复读压缩、有界渐进构建、完整 bank/crop 发布；SHP/VXL 缓存检查了 source/palette/type/owner/kind/variant/pose/HVA 身份、当前/显示 fallback 保留、批量失效扫描、时间与内存预算、瞬态资源过期。GPU 只在整帧 fence 与 readback 成功后提交缓存；失败使用同一保留绘制记录回放 CPU。超时和 host 故障可隔离后端，toggle 不重新建立已隔离驱动；仍可能在飞的资源不会过早销毁。

当前仍有独立 fresh/live 读取：视口、部分遮罩/本地 UI，以及父任务指出的乘员气泡在外层 WorldFrameScope 结束后调用 `ReadSession::read`。**不能据此宣称所有显示消费者都已封闭样本化，或所有高频重复采集已消除。** 本轮没有量化这些剩余读取的 RPM 次数或真实帧成本。

主/副相机/小地图/监视器的结果提交检查了当前连接、match/generation、profile、view/尺寸及年龄；关闭/暂停先撤回显示与失效 generation，再停止 worker。Latest worker 的 invalidate 不抢占正在执行的读取/绘制；shutdown 要等待 worker，真实关窗延迟未测。GPU fence 的普通硬件等待预算为 100ms，也不等于所有驱动 API 调用都能在该期限内返回；隔离的未确认资源可能保留到进程退出。

## Original / asset 替换入口穷尽检查

`rg -n '\basset\s*=' source/cursor/source/main.cpp` 的生产赋值只有两处：

1. **`loadCache()`**：先 `streamShapeGate.reset()`，再 `asset=decode(original)`。attach、目录/cacheKey 切换和读取失败均经过该复位点。
2. **`pollOriginalResource()`**：新原生 SHA 通过核验且 canonical 内容变化后，先 reset，再移动替换 `original` 和 `asset`。

`importStyle/chooseStyle/rebuildDisplay/resetPrepared` 修改的是 `displayAsset`、显示帧或补间 bank；形状门读取原始 `asset`，这些动作不要求无谓地丢弃原始形状判定。安装/恢复 SHA 修改游戏资源文件，后续实际原始缓存重载或新资源采集仍经过上面两处入口。没有发现以相同 frame number 跨 original 替换复用旧判定的第三个赋值入口。

## 本轮实际验证与不能外推的范围

| 证据 | 实际执行结果 | 能证明的范围 |
|---|---|---|
| `cursor-shape-before.json` | 旧生产函数 UBSan 报错，退出 1 | 大画布溢出可复现 |
| `cursor-stream-after.json` | 修复生产头文件 ASan+UBSan，退出 0 | 形状门及判定缓存的受控输入行为 |
| `cursor-shape-validation.json`、`cursor-main-x64-compile.log` | CursorEntry 真实翻译单元 x64 交叉编译退出 0 | Cursor 修改可编译 |
| `overlay-paused-gates/*/result.json` | 真实 sampler/Motion 与原文 count 语句，18 项前后检查 | F3 的更新/绘制新鲜度一致性与失效边界 |
| `idle-aircraft-paused-gate/result.json` | 实际 sampler 与原文按钮条件，编译/运行退出 0 | F4 显示年龄门缺陷，尚未修复 |
| `tooltip-fixture-build-attempt.json` | 独立夹具链接失败，未执行 | 明确限制，不能计通过 |
| `final-build/logs/build-provenance.json` | 完整交叉编译、静态 PE 验证均退出 0；源文件与 HEAD 前后相同 | 最终提交的 Host x64、Runtime/Broker x86 集成可构建与 PE 静态检查 |

最终构建产物是 `RA2Toolbox.exe`、`GameRuntime.dll`、`GameRuntimeHost32.exe` 及配套 loader；构建 provenance 的 `runtime_tested/game_tested/multiplayer_tested` 均为 false。没有真实 Windows 运行、真实 GPU 驱动故障、画质、帧率、多屏交互或多人联机验证。

Cursor 修复验证最初的 LeakSanitizer 因 executor 无法枚举 `/proc/.../task` 失败，原始失败记录保留。随后仅设置 `ASAN_OPTIONS=detect_leaks=0`，AddressSanitizer 和 UBSan 仍启用并退出 0；不能宣称泄漏扫描已通过。其它旧测试只作为源码/历史参考，没有为扩大覆盖率重复执行，也没有把测试文件存在写成执行证据。

最终逐项清单合并了 `render-root-supplement.json`、`render-root-options-supplement.json`、`render-hardware-supplement.json`，并对 SinglePlayerFPS 引用硬件矩阵的同一实现审查。父任务负责后续归档；本审查未修改版本、INDEX、远端或自行提交。
