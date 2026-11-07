# 硬件控制与工具箱计算：准入重读、统一采样内的重复工作

## 问题、基线与证据范围

用户报告开启硬件控制敌方单位后严重卡顿，并补充全部选择工具箱计算仍会卡，追问采样是否已经由游戏统一完成。本轮确认统一游戏线程采样仍在；确定缺陷位于准入复核、无实际需求时的整轮工作准入、同帧躲弹威胁收集，以及一次采样内部按攻击者／目标组合重复读取 selector 数据。修复没有改变统一逻辑帧采样频率，没有新增各功能独立采样器。

基线为稳定 1.0.43 `bf3fbe88faa96513649f2998aa0ab6e71935f4a8`。调查前已查 INDEX、[统一游戏帧采样](2026-10-31-coherent-compute-frame-hotfix25.md)、[单槽控制路由](2026-11-04-control-hardware-parameters.md)、[既有断点热路径优化](2026-11-04-breakpoint-hotpath-performance.md)及同一持久 SQLite 笔记 v157。所改既有文件的基线均按 Git blob SHA 核对并保留。

继承支持样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。地址、字段和 ABI 依据来自上述既有研究及当前 `raw_combat.h`，本轮没有访问私有 EXE／dump、重新反汇编、增加游戏地址或修改共享协议 ABI。证据层次是源码审查和实际执行的受控 C++ 夹具；用户反馈没有伴随可用于归因的现场命中率／帧时间记录。本笔记不预告产品整包编译、Windows、游戏或联机验证通过，后续构建包另行记录。

机器可读摘要、最终源码散列、复现命令和原始收据 SHA-256 见[相邻证据 JSON](2026-11-08-hardware-control-and-host-compute-hotpaths.evidence.json)。

## 一、控制准入的重复读取，不是每次 VEH 的读取

`tech_ui_breakpoint_bridge.h::controlCompatible()` 在 VEH 回调之外复核入口、两个 caller 上下文和原生叶。基线先做完整可读性预检，再调用重复读取同一范围的原始字节与严格兼容谓词；一次成功复核实测为 24 次 `read` 边界调用、累计 235 字节。这里的「准入」不保证调用频率低：既有请求／维护路径仍可能反复进入，本轮没有改变它的触发频率。

新增 `controlparams::AdmissionImage` 每次复核重新读取以下三个范围，再让原有 `mastercontrol::original()`／`compatibleStrict()` 对同一局部图像工作：

| 起始地址 | 长度 | 覆盖内容 |
| --- | ---: | --- |
| `0x50B6F0` | 52 字节 | 控制入口、已核实前缀／函数体、true 与 strict 原生叶 |
| `0x6339B0` | 28 字节 | owner caller 前缀、原 CALL 与返回后上下文 |
| `0x637E37` | 27 字节 | planning caller 前缀、原 CALL 与返回后上下文 |

三段恰好是旧检查所需字节的并集，没有多读未验证间隙。成功复核变为 3 次 `read` 边界调用、107 字节，调用数减少 87.5%。正式 `read` 绑定为安全进程读取；夹具只在受控边界计数，没有执行真实 RPM 系统调用或测量其时间。

任何一段不可读都会使整个图像无效，失败重采不能复用旧内容；早段签名不符仍读取余段以判定完整可读性。两种既有 forward 入口仍走原严格谓词。`Breakpoint` 与同地址 `Patch` 的 debug 策略继续独立，签名绕过不能授权不可读入口。没有跨调用、同帧或跨帧成功缓存。

同时把 `debugentries::index()` 的不可变目录身份查找改为编译期 512 bucket、1 KiB 索引。动态 skip 列表、策略世代和观察状态仍即时读取；只有 ID 到原目录行号的映射被索引。对当前 161 行目录作静态操作计数，控制断点 key 从 158 次 entry 比较变为 1 次 bucket 探测；全部已知 key 各查询一次由 13,041 次比较变为 188 次探测，最多 4 次。此处比较的是算法工作量，不是计时。

**实际控制回调未改。** 成功 tag4 路径的世界 10 次、目标 House 身份 6 次、返回地址 1 次即时本地字段读取均保留。基线早已使用 SEH 范围内的直接读取，不能把本轮 24→3 写成「每次硬件异常少 21 次 RPM」。严格返回地址 `0x6339BF`／`0x637E45` 仍转原生严格比较叶 `0x50B716`；其他通过验证的 caller 仍转 true 叶 `0x50B710`。stop/fault、世界／House 身份、原所属方、foreign DR 所有权、撤回恢复和四槽预算不变。

相同配置的 Service 已有 settled 快路径及约 500 ms DR audit 节流，不能把基线描述成每轮都重新安装断点。本轮没有改 `configure()` 的权限发布、事务或维护周期。

## 二、统一采样与计算位置

共同游戏线程仍发布 `computeFrameSeq/Serial/Match/Tick/Frame`；Host worker 消费固定输入、保留来源并返回结果，游戏线程保留最终身份、所属方、手动令牌、时效和事件提交检查。「工具箱计算」不意味着游戏线程不采样、不维护、不执行最终合法性查询。

`bridge.cpp` 原先把 `computeApplied != compute::All` 当成新帧进入整轮工作的充分条件，即使选在 Host 的功能没有启用或任务。现在 `hostFrameCaptureWanted()` 只补充实际 Host 启动需求：自动维修、自动卸矿，以及当前世界内仍有效、需要首次输入或明确请求刷新输入的协作意图。原有实时功能／任务需求继续保留。

移除这一泛条件时，显式补入装车 ledger、维修候选／ACK／待观察事件、协作 reservation／effect、mission observer、actor lease 和 ownGround 在途任务，避免原先依赖泛条件的工作变慢。同帧 `cmd` 和 `resultsDue` 仍可绕过 cadence；stop/fault、硬件即时停用及 world capture／retirement 没有被移到采样门后。

**剩余唤醒不能省略不报：** `main.cpp::setFrameSampling()` 仍因 `computeApplied != compute::All` 请求帧通知。当前修复阻止无实际需求时进入整轮重工作，并未完全取消这类通知及其前置维护／world capture。没有据此声称无需求 Host 配置已成为零成本。

## 三、Host Scatter 躲弹的同帧威胁收集

基线 `dodgeControlTick()` 在发现没有新 Host 输入帧之前，已调用 `collectDodgeThreats()` 遍历弹丸数组 `0xA8ED40` 和单位数组 `0xA8EC78`；同一帧的重复消息与 2 ms fastDue 唤醒也会重复这项工作。

现在 Runtime 路径、新统一输入帧及同步 reverse 路径保留采集。**Host Scatter 模式 1** 的同帧重复调用，仅在一个稳定新结果有保留的来源输入、即将应用时补采威胁表。`updateDodgeJobs()` 仍在原暂停门之后执行身份／手动版本移除、运动观察和 lease 维护，不因没有新样本而跳过。

局部幂等 `collect()` 同时放在实际结果消费点与 reverse 消费点。因此结果在维护期间发布、模式在维护期间切为 0，或带旧 job 的 disabled 状态在维护期间重新启用 reverse，都会在消费前得到新表；已有本次采集不会重复。独立审查发现的 reverse 消费点竞态已据此修复并纳入两例受控检查。

受控实际执行的 100 次同帧 Host 调用包含一次新输入：威胁收集 1 次、身份／lease 维护 100 次、输入发布 1 次。没有稳定 seqlock 或没有来源输入的结果不会导致额外全表采集。固定基线函数的负对照在首个「无新输入／无新结果不应扫描」断言失败；这不是基线完整 100 轮计时记录。

**reverse 模式 0 仍走原同步 `reverseDodgeTick()`，绕过 Host 计算分支。** 本轮保持该既有行为，不声称每种躲弹模式都服从 Host 位置或完全移除了游戏线程算法。

## 四、一次 combat 采样内部复用 fresh selector

旧 `combatWeaponQuery()` 对每个攻击者／目标组合重新执行 `rawCombatInfo(source, ..., true, cached=false)` 和 `rawCombatInfo(target, ..., false)`。此前构造的 `World::Actor` 使用 `cached=true`，武器元数据可保留至 30 个逻辑帧；直接拿这个 Actor 做 `choose()` 会改变选择语义，因此未采用。

新 `CombatProbeBatch` 仅存在于一次 `collectCombatProbes()` 调用中。同一段不调用游戏例程、不写游戏字段的 raw 循环，source 按 pointer/type/kind/epoch 取得一次原 fresh 表示；target 按 pointer/type/kind/epoch 取得一次原 fresh 表示。每个 source 的 `worldByID` 仍执行一次，每个 pair 的 target `worldByID`、双方 kind/type 和 source disabled 检查仍即时执行。

| 阶段 | 保留的数据与语义 |
| --- | --- |
| source selector | 仍从零初始化 Actor，执行原 `rawCombatInfo(..., true)` 默认 `cached=false`，再读 XY；武器槽、stage/deploy/elite、flags、XYZ、airRangeBonus 和 fresh 武器信息沿用原 reader |
| target selector | 仍执行原 `rawCombatInfo(..., false)` 再读 XY；紧凑 scratch 保存 kind、armor、XYZ、flags，覆盖 `weaponIndex/choose` 的完整目标依赖 |
| 每个 pair 的选择结果 | 仍重新解析 `rawWeapon(type, current veterancy, selected slot)`，保留指针、slot、close 和 airborne 判定 |
| 最终 compatibility | 仍复制原发布的 World source/victim，使用选择后的 FixedWeapon 和 `rawWeaponInfo(selected, true)`；原安全 flags、effectiveness、最小射程及额外获取范围检查不变 |

raw 循环只修改自己的 scratch、World、probe coverage、cursor 和既有 WeaponCache。此论证不扩展到更早的 World 构造：其间可能有其他原生地图查询，不能因同属一逻辑帧就拿那份 Actor 充当 fresh selector。

进入显式 `nativeCombatQueries` 分支前使 batch epoch 失效，成功／零结果／故障均如此；下一次 raw pair 必须重新采 source/target。native 虚函数调用仍逐 pair 执行。每次 publication 重置 target epoch 并建立新 batch；独立 `probeCombatPair()`／`advancedTargetInRange()` 不传 batch，仍即时读取。static scratch 只避免约 576 KiB 栈分配，其数据不能跨 publication 直接复用。既有 30 帧 WeaponCache 与这份批内对象观察严格分开，策略未变。

受控固定基线对照在 128 个 source × 256 个 target，即 32,768 pair 下得到：

| 操作计数 | 基线 | 当前 |
| --- | ---: | ---: |
| source `rawCombatInfo` | 32,768 | 128 |
| target `rawCombatInfo` | 32,768 | 256 |
| `rawWeaponFresh` | 65,536 | 256 |
| `get<T>` 字段读取 | 4,288,808 | 350,696 |

这些是实际执行夹具的函数／字段读计数。生产 `get<T>` 是本地字段读取；受控字段读次数不能写成 RPM 系统调用次数，更不能换算 Windows 时间、游戏 FPS、硬件断点命中量或联机正确性。

17 组 raw 分支对照覆盖武器槽／部署／stage／elite、装甲／飞行、特殊弹头、弹药、ID 拒绝、非法射程，以及 fresh selector 与既有元数据 cache 不同的情况；新旧 probe flags、slot、effectiveness、Actor coverage 逐字节相等。另验同帧再次 publication 重采、native 成功／零／故障、source 与 target 的 raw→native→raw 失效，以及 standalone 不缓存。输出一致性与上述依赖审查共同支持批内复用，而非只凭一组基准判断等价。

## 五、保留的扫描与硬件异常成本

`publishPlanningWorld()` 的 AntiTheft 建筑预扫描仍在原位置：它遍历 `0xA8EB40` 并读取当前所属方、存活、HP、limbo 和 `antiTheftBuilding()` 的 feature config。并非所有同帧变化都会更新 `inputSerial`，现有 config invalidation 还部分依赖 miner-defense 条件。把 planningFrame 门提前或仅按 frame/flags 缓存「没有合格建筑」，会遗漏同帧新合格对象／过滤设置变化；本轮没有采用，也没有声称所有采样遍历已经消失。

控制仍拦截公共入口 `0x50B6F0` 的每次命中。其后分流到多个原生叶，单独后移会漏路径，只拦特定 UI caller 会缩小公共控制语义。现有 native 证据不足以给出单槽、无覆盖空窗、保持所有 caller 语义且命中更少的替代站点，本轮未猜新地址。没有按帧跳过世界守卫、轮换／暂停 DR、覆盖 foreign 槽或自动回退代码 Patch。

统一 sampler／raw planner 未发现直接调用该公共入口；源码中的直接执行引用是与硬件模式互斥的 native control 回落 thunk，以及专用出售光标的原谓词回放。可选 native combat 虚函数是否间接再次进入公共控制谓词，当前证据不能确定或量化。因此不能把已证明的重复 combat probe 直接说成已证明的重复 DR 命中。

`controlExceptions` 已有 owner-thread 普通递增，但未导出到 Shared／UI／日志；现有 `controlStatus` 日志不能恢复每秒命中率。本轮未扩协议、未添加高频日志。既有 Windows 自有进程基准记录的是简化 DR／VEH 链，不能当作本轮生产 classifier 或游戏帧时间。CPU 调试异常、Windows 派发与 CONTEXT 恢复仍逐次发生；去掉准入和采样冗余不等于消除这项固有成本，更不构成严重卡顿已全部根治的证据。

## 验证收据与复现

以下均为本轮真实执行的 Linux C++17 受控夹具，不是继承旧测试数字。尤其 2752 项 callback 收据包含本轮更新后的 policy/header SHA。原始路径以工作根目录为基准，完整内容摘要及文件 SHA 已写入相邻 evidence JSON。

| 验证 | 结果 | 原始收据 |
| --- | --- | --- |
| 抽取新旧生产准入函数、逐字节签名／不可读／独立 DR 策略 | 2228 项通过；24→3 次边界读，235→107 字节 | `hw_control/validation/hardware-admission/result.json` |
| 不可变目录索引、独立策略、无效输入与真实观察状态 | 5407 项通过 | `hw_control/validation/debug-catalogue/result.json` |
| 实际控制回调、严格 caller、无数据写入、拒绝路径与槽预算 | 2752 项通过 | `hw_control/validation/control-callback/result.json` |
| 实际 dispatcher/dodge 函数与固定基线负对照 | 当前通过；旧函数首项预期失败 | `hw_control/compute-audit/final-receipt.json` |
| 实际 raw reader／collector 与固定基线函数 | 17 分支及额外边界通过；操作计数如上 | 同上 |

从仓库根目录复现硬件验证：

```sh
python3 source/assist/tests/debug_breakpoint_policy.py --build-dir /tmp/hardware-admission --baseline-bridge /path/to/bf3-baseline/source/assist/source/tech_ui_breakpoint_bridge.h
g++ -std=c++17 -O2 -UNDEBUG source/assist/tests/debug_entry_policy.cpp -o /tmp/debug-entry-policy
/tmp/debug-entry-policy
python3 source/assist/tests/control_parameters.py --out /tmp/control-callback
```

复现计算验证：

```sh
python source/assist/tests/runtime_sampling_hotpath.py
python source/assist/tests/combat_probe_sampling.py --baseline /path/to/bf3-baseline/source/assist/source/combat_probe_bridge.h
python source/assist/tests/runtime_sampling_hotpath.py --baseline /path/to/bf3-baseline/source/assist/source/dodge_bridge.h
```

最后一条是应失败的负对照。`/path/to/bf3-baseline` 必须来自固定 `bf3fbe88...`，原执行路径与收据 SHA 保留在 evidence JSON。此处只编译并执行这些受控测试；没有以它们代替正式产品构建、Windows 回归、用户实机 FPS 或联机验证。独立只读复核已覆盖需求、在途任务、同帧结果／模式切换及 combat 字段／类型／失效边界；文档写入时另核对所有收据记录的源码 SHA 与当前文件一致，未重跑额外测试套件。
