# pre.5：公共权限验证与实时对象身份读取

基线：已发布 pre.4 `061e106b71c81d79ea687edf36ad8401ffb722f8`。只修改自有 Runtime 基础设施；对象 ID 沿用现有 `+0x10`，没有新引擎地址、偏移、ABI 或可执行样本结论。Shared/协议和公开导出不变。

## 发现与修改

- `worldByID` / `rawObjectID` 在封存作用域外经 `read` 走本进程 ReadProcessMemory，每次只复核四字节。新 `readObjectIdentity` 保持采样域选择：在 SampleReadScope 内继续读封存页，失败不补实时；在作用域外每次调用 `GuardedReadObjectIdentity`，用本地 volatile 读取当前 ID，没有缓存成功结果。仅局部 AV/in-page 失败返回 false，恢复 LastError；其他异常沿原调用者/平台处理。这个入口不新增执行或写入权限。
- `gameentry::active` 原先每次构造 108 字节 Request，通过完整管理分发；稳定成功路径涉及模块缓存、活动令牌与管理器 owner 字段的四次 InterlockedCompareExchange 读取。
- `validateCurrent` 是同一管理器的内部函数，不是公开导出。每次仍比较真实线程、模块、客户端和最内层 token。只有当前线程/模块匹配后才读取 owner 线程独占的 Policy。失败走原 ManageGameEntry 验证路径，保留 Busy/WrongOwner 和诊断行为。
- 模块缓存、令牌和 owner 发布改用始终 lock-free 的 32 位 std::atomic acquire/release；不把 volatile 普通加载冒充原子同步。用于抢占执行权的 busy CAS、争用计数、补丁事务和 LIFO 管理仍保留。两个科技私有 probe 的令牌读取也使用相同 acquire helper。

## 证据

- `entry_hotpath.py` 抽取真实旧/新 ManageGameEntry 与新 validateCurrent，使用真实 Policy 和客户端头。Win32 API、线程身份和冷补丁操作为显式 seam；管理入口仅插入调用计数。共同语义摘要 `2040305750` 一致。100000 次有效验证的 Interlocked 调用 400000→0，完整请求分发 100000→0；覆盖 128 层捕获、错误线程/客户端/模块、旧 token、过深捕获、重入拒绝、死亡线程重新绑定和异常清理。不是 Windows 时间或游戏 FPS。
- `object_identity_read.py` 对照真实旧/新 worldByID/rawObjectID 路由，510 检查、摘要 `1030470243` 一致。200000 次身份读取全部保留，模拟 RPM 边界调用 200000→0；本地保护读取变为 200000。地址复用、即时 ID 变更、缺失封存页、嵌套采样域、无索引对象和读取失败均覆盖。
- `runtime_scope_windows.py` 编译真实管理器、guard.cpp 及实际 MSVC x86 SEH 边界。新增实际内存保护、跨页、释放后读取、LastError、UID 变化和恢复检查；100000 次有效权限验证、10000 次实际错误线程拒绝；原有普通返回、外层 AV 展开和恢复执行的 debug 异常继续验证 LIFO/FP/cleanup。运行结果由 pre.5 工作流保存。
- 同一 Windows 夹具交错三轮比较 ReadProcessMemory 与局部读取，每轮各 50000 次；只记录读取微基准，不设置时延门槛、不外推游戏帧率。
- `tech_parameter_lookup.py --current-only`：132826 检查、322 回调用例；只说明当前回调/令牌 seam 通过，不把 pre.4 当作尚未优化的历史基线。
- `harvester_defense_runtime.py`：72 场景、2957 检查，原生保护读为明确的模拟内存 seam；真实 guard 另由 Windows 夹具验证。

## 保留的成本与范围

没有降低采样频率、候选容量或身份复核次数，没有延长命令寿命，没有把游戏原生函数挪到后台。公共 call/put 使用更轻的权限验证；硬件异常命中次数及 CPU/Windows 的异常派发成本没有在本轮消除。控制快照仍先在 Runtime 捕获，再完整复制控制页到共享发布槽；本轮没有改页面 wire 格式或这一发布架构。
