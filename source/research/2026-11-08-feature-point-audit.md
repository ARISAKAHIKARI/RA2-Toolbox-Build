# RA2 Toolbox 逐功能点补充审查

## 结论

此前的 29 行模块表不能证明每个功能点都已审查。本轮重新按实际 UI、快捷键、协议命令、自动开关及计算模式建立逐项记录，并追踪入口、实现、高频工作、身份与时效、关闭/换局、失败重试等路径。四组清单中已没有待追踪的源码条目；原跨组委派的四项底层实现另有交叉核对记录。

这表示**本清单所登记范围的源码审查已完成，不表示全部功能已运行通过、全部问题已修复或所有组合已验收**。7 处问题已作源码修正；仍有确认未修复的问题及源码边界候选。源码、受控执行、编译/链接和真实 Windows/游戏验证分别记录。

基线：`c3154fa343a5be42da3453c0b3d228dea653d3ac`。修复源码：[GitHub commit d9d7056c6e0f](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/d9d7056c6e0faeef60507f8c2c7f7c847dc8af26)。
本地验证提交：`491bbc72ec67ac70454e9d9a0ef69287eb1ba28f`；两者完整 Git 根树均为 `ced57e4a7a3a2e452683350c60768f8e3ef91aa7`，`source` 子树为 `034f6479572e6c4c542804d1a99d36ec56e2cb33`。远端 API 生成了不同提交身份，未更改源码字节。后续报告提交仅增加本报告、清单、索引及证据，不属于新的生产修复。

## 清单如何计数

| 范围 | 审查记录数 | 完整明细 |
| --- | ---: | --- |
| 战术、自动化与快捷键 | 76 | [tactical-matrix.json](evidence/feature-point-audit-20261108/tactical-matrix.json) |
| 显示、光标、监视与渲染 | 152 | [render-matrix.json](evidence/feature-point-audit-20261108/render-matrix.json) |
| 科技、硬件入口、地图、超武与诊断 | 82 | [hardware-matrix.json](evidence/feature-point-audit-20261108/hardware-matrix.json) |
| Host、控制台、配置与共享机制 | 44 | [host-matrix.json](evidence/feature-point-audit-20261108/host-matrix.json) |

这些是**审查记录数**，有共享机制和跨组交叉，不能相加称作独立产品功能总数，也不是运行通过数量。[人读逐项清单](2026-11-08-feature-point-audit-checklist.md)列出每行功能、模式及状态；JSON 保留实现路径、证据、发现和验证缺口。

- 战术清单逐一对照 34 个非 None 协议 Command、32 个 Flags、13 个自动开关、15 个计算槽（14 个当前可见）。已退役的 AutoCS、步兵强制进驻、刷弹及旧切换玩家命令只记禁用/拒绝路径。
- 显示组包含 68 个共享显示项、40 个光标项、21 个监视项、16 个底层显示路径和 7 个独立 neural helper 项。保留配置与后台管线不是同等数量的当前可开关用户功能。
- 硬件组包含科技/地图/控制的独立模式、快捷键、21 个诊断步骤与生命周期；82 条审查记录与目录恰好有 82 个站点只是数字巧合。
- Host 组覆盖启动、控制台操作、设置写入与关闭、进程选择、输入与 Broker、共享 worker 等 44 项机制。

## 7 处已修正问题

### 1. 批量维修结果重复全表查找

`bridge.cpp::repairCalculationTick` 原先对每个 Host 结果 ID 扫描整个 Runtime 保留候选，最高形成 8192×8192 的嵌套查找。现在对 Runtime 私有固定缓冲排序一次，再二分匹配；保留返回顺序、重复项与后续维修 toggle 去重。候选数量由 Runtime 生成并限制到 8192，缓冲沿用现有单 Runtime 工作入口。

真实生产函数提取后在受控内存执行 **28 场景、101 项检查通过**，覆盖满容量、未知/重复/零 ID、空集合、关闭后迟到结果和换局。[执行记录](evidence/feature-point-audit-20261108/repair-result.json)

### 2. 智能卸载历史重复线性匹配

`native_orders_bridge.h::observeUnloadRoster` 原先每次观察乘客都线性扫描历史。改为固定容量索引，并在历史过期、交换删除后重建；继续保留同帧观察频率、历史顺序和淘汰语义，避免破坏原生先发命令后的识别。

实际提取旧/新生产函数逐步比较历史字节、顺序与数量，**5 场景、83 项检查通过**，含碰撞、容量上限、交换删除、手动 token 与关闭门槛。合成用时不代表游戏 FPS。[执行记录](evidence/feature-point-audit-20261108/unload-roster-result.json)

### 3. 未启用帧诊断仍逐消息读取与探测

`frameobserver::maintain` 将游戏就绪读取移到确实需要新安装显式诊断入口时；通道尚不存在时按 owner 每 100 ms 发现一次。撤销选择、控制器死亡和所有权清理仍立即处理。已安装入口遇到短暂过期 heartbeat 的既有保留语义不变。

同一虚拟一秒、100000 次维护的生产函数对照中，未创建通道时读取调用从 600000 降为 0，映射探测从 100000 降为 10。坏头、迟开、关闭、死亡、恢复失败、部分写入后清理、策略变化和 tick 回绕的语义断言通过。100 ms 是发现间隔，并非系统调度的绝对时限。[修复前](evidence/feature-point-audit-20261108/frame-maintain-before.json) · [修复后](evidence/feature-point-audit-20261108/frame-maintain-after.json)

### 4. 合法光标尺寸触发有符号溢出

`cursorstream::plausible` 对合法 128×128 全不透明输入已有平方乘积溢出，UBSan 实际报错。乘积在平方前提升到 `long double`，保留原阈值。55/128/256/512 尺寸、近/远/空形状、空指针与非法 stride 等修复后检查通过。[实际溢出记录](evidence/feature-point-audit-20261108/cursor-shape-before.json) · [修复后运行](evidence/feature-point-audit-20261108/cursor-stream-after.json)

当前导入 UI 限制为 55×43，大画布触发依赖解码器接受的原始缓存或旧采集路径；本轮未证明默认光标会触发该溢出。

### 5. 光标同一帧反复扫描完整形状

新增当前相邻帧判断缓存，连拒绝结果也缓存；源帧变化重新计算，两个原始资源替换入口在替换前复位。样式资源与原始形状资源的生命周期分开核对。受控执行 10000 次同一帧对仅一次形状扫描，换帧、换原始资源与失败缓存行为均通过。[源文件与编译记录](evidence/feature-point-audit-20261108/cursor-shape-validation.json)

### 6. 暂停后有效敌方/状态标记被绘制门隐藏

采样方允许同一不可变世界帧在获得新近确认时继续展示；原绘制门却只接受不足 250 ms 的采样，导致敌方、寄生、箱子增益等标记在暂停后消失。更新与绘制现在共用仅用于显示的 `displaylease::frameCurrent`：同局同帧仍获确认时保留；换局、换帧、失去确认、关闭及非法计数仍拒绝，不刷新获取时间，也不延长命令授权。

真实 sampler、motion 与从 `main.cpp` 原文抽取的绘制计数门执行 **18 项检查通过**；旧版暂停到期后计数为 0，新版保留有效样本。[基线对照](evidence/feature-point-audit-20261108/overlay-before.json) · [修复结果](evidence/feature-point-audit-20261108/overlay-after.json)

### 7. Hook 诊断日志目录与协议文案过时

旧文案写 Assist89、14 个实验站点；当前生产协议为 Assist101，目录实际为 48 个覆盖、3 个禁用、1 个 Cursor 专属、30 个显式实验站点。日志现取协议常量及分类函数计数，删除固定数字。目录执行枚举 `missing=0`，只证明站点分类有归属，不证明 21 个诊断步骤都在游戏执行。[目录计数](evidence/feature-point-audit-20261108/catalog-count.json)

这项日志修改经过源码复核；本轮完整产品构建没有包含独立 Hook Diagnostics helper，也没有执行真实 Win32 日志生成，状态保留为 `source_fixed`。

## 尚未闭合的问题

### Broker 启动/恢复时占用共享锁等待——受控确认，未修改

生产 `Client` 与从当前源码提取的 Host 路由在启动/重连等待期间持有共享互斥量。两条等待边界分别由另一条真实 C++ 线程争锁，两次都无法取得；健康状态的 100 次 pulse 没有正等待。8 项检查通过。

注入的 2990+1990 ms 应答延迟产生 4980 ms **虚拟时间**，不是实测卡顿时长，不证明正常游戏帧掉帧或死锁。修复需把启动/恢复所有权和目标代数移出共享锁，同时保留旧进程与 mailbox 退休语义；不能简单删锁或提前关句柄。[受控证据](evidence/feature-point-audit-20261108/broker-lifecycle.json)

### 暂停后空闲飞机按钮隐藏——受控确认，未修改

真实 sampler 同帧/相机刷新仍保留原 `idleAircraftTick`，原按钮完整条件在 500 ms 边界后将其隐藏。独立复核发现不能直接套用上面的 250 ms 标记策略：飞机计数受 100 ms 节流，没有自己的局/帧戳；失败回滚也未覆盖这些字段。必须建立已提交的展示样本身份并处理节流间隙，不能把旧计数重新标成当前帧。

点击仍有独立的 500 ms 样本门、命令通道时效、PID/match 与 Runtime 对当前飞机状态的复核。未靠更新时钟延长命令许可。[复现](evidence/feature-point-audit-20261108/idle-aircraft-paused.json) · [独立源码核对及修复边界](evidence/feature-point-audit-20261108/idle-aircraft-independent-review.json)

### 建造提示框满 75 项边界——源码发现，未修改

`build_time_tooltip.h` 在 `count >= 75` 时退回原文，侧栏枚举与新增上限允许恰好 75 项。这是源码可见的边界不一致；尚无本轮受控或真实游戏满栏复现，不能报运行验证完成。[完整路径与边界](evidence/feature-point-audit-20261108/render-hardware-supplement.json)

### 生产预定跨世界重标——Host 路径受控确认，未修改

`serviceCommandQueue` 按旧对局身份验证并弹出请求后，`send` 内再次 `readSnapshot` 可读到新对局。生产预定没有拒绝这次身份变化，随后以新的 `commandMatch` 写入 `requestMatch` 或重新排队。实际提取这三段生产函数及生产 `Book::cancel` 执行 **6 场景、48 项检查**：旧 match=7 的 ProduceDeferred 与 ProduceReserved 被发布为新 requestMatch=8；后续取消旧 book 不能撤回已经发布的 request。新局 busy 时两者也会以 match=8 重新入队；改变类型目录时则正确拒绝，作为反例对照。

这是 Host 重标/发布缺陷的受控确认。没有执行 Runtime 原生生产；Deferred 没有旧父建筑 UID 防线，而 Reserved 的旧 UID 复核可能拒绝，因此不能宣称两种命令都已在真实游戏中跨局生产。本轮未修改该实现。[专项复核](evidence/feature-point-audit-20261108/reservation-world-boundary-review.json) · [6 场景受控结果](evidence/feature-point-audit-20261108/reservation-world-boundary-result.json)

### 副屏统计拖动边界——未证实的源码风险

`beginSecondaryDrag` 可能取得新显示器工作区与尚未重排的旧面板尺寸，后续 `std::clamp` 的上界没有显式钳到下界以上。跨不同尺寸显示器时的实际事件可达性尚未受控或实机复现，保留 `R-MONITOR-DRAG` 为风险，未记作已确认运行缺陷或已修复。[相关逐项记录](evidence/feature-point-audit-20261108/render-matrix.json)

## 范围澄清

- Passenger bubbles 仍有独立的 live ReadSession 扫描；手动进程识别规则为成功写入后再发布策略的同步保存。没有测其实际 I/O 或帧耗时，不能说所有消费者都接入不可变世界帧，也不能说所有设置入口都已异步。
- RenderUpgrade 当前 UI 入口受禁用策略限制；若干旧画质配置虽保存，有效值固定为兼容恢复模式。物理像素偏好不能证明独立 4K 重建；当前 xBRZ 不是可开启的有效产品缩放。neural renderer 是保留的独立 helper，没有当前 assist/suite 接线。
- 高刷移动的实际实现是基于最新两次观测的有限展示位置前推，最多一个观测间隔/25 ms，并有身份与突变门槛；不能只照注释称其完全不预测位置。未改变游戏时钟、SHP 帧或命令。

## 实际验证和失败记录

修复源码执行了针对性生产函数/头文件受控验证，并完成 x64 Host、x86 Runtime/Broker 的完整交叉编译与静态 PE 校验。构建于 `2026-11-08T11:01:47.469065+00:00` 开始、`2026-11-08T11:07:45.694976+00:00` 完成；所有生产输入哈希和 HEAD 前后保持一致。静态检查涵盖架构、导出、协议/Shared 大小、版本、WebView2 loader 及外部依赖。

完整[构建来源记录](evidence/feature-point-audit-20261108/build-provenance.json)、[简表](evidence/feature-point-audit-20261108/build-summary.json)与[PE 输出](evidence/feature-point-audit-20261108/pe-verification.log)保留了具体输入和产物哈希。文件版本仍是 `1.0.44.5`，未重新分发同名更新包；先前 pre.5 下载包不包含这轮后续修正。

需要保留的失败/限制：

- 光标验证首次因环境无法枚举 `/proc/.../task` 而 LeakSanitizer 失败；随后仅关闭 leak 扫描，ASan 与 UBSan 继续启用并通过。不能称内存泄漏验证通过。
- 既有 `cursor_tooltip_windows.cpp` 独立编译前端通过、链接因缺 `_RuntimeScopedCall` 失败；没有运行，没有用假 scope 替代。集成 Runtime 构建成功不能把这个独立 fixture 改记为通过。[失败记录](evidence/feature-point-audit-20261108/tooltip-fixture-build-attempt.json)
- 没有原生 Windows loader、真实 GPU/驱动、多屏/DPI、实际游戏动作/画面/声音、硬件 #DB/VEH 或多人联机验收。没有测真实 FPS；未执行的既有测试只作为源码参考，不算本轮通过。

## 证据与后续验收

[证据目录索引](evidence/feature-point-audit-20261108/manifest.json)保留文件哈希；历史执行命令中的绝对路径照录，不表示文件当前必须位于同一路径。受控验证驱动随源码位于 `source/assist/tests`；四组 findings 和矩阵记录各自范围。

本轮新结论来自工具箱源码、受控边界和交叉编译，没有新的游戏二进制反汇编、地址或 ABI 发现，也未读取/发布新的私有游戏可执行文件或转储。先处理已确认的生命周期、显示与跨世界发布缺陷，再对有改动的行为完成对应 Windows/游戏验收；此报告不将这些后续工作计为已完成。
