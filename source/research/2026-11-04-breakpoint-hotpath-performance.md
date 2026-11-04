# 硬件断点热路径：即时签名核验、归属分类与诊断计数

## 问题与证据范围

用户报告每分钟数十万次硬件断点命中，要求尽量精简回调。基线为
`48d283ae3f854d94ae2568136e8c6e5ef62909e1`，先查研究 INDEX、控制/科技/单槽黑幕笔记和持久 SQLite v123。
本轮是源码审计、受控辅助函数与自有 Windows 进程检查，没有启动真实游戏、物理 GPU 或联机对局。

继承支持样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
沿用现有入口及 ABI：CanBuild `0x4F7870`、科技比较 `0x4F7951`/`0x4F7B66`、控制 `0x50B6F0`、黑幕显示 `0x6D88FC`。
本轮没有重新反汇编私有映像，没有新增地址或扩大版本支持。

## 审计结论与修改

`bridge.cpp::get<T>()` 本来就是本地解引用；不能把 Map/world 的十余次字段读取误报成 RPM 系统调用。
Map 之前已经本地检查世界身份及十字节前缀。Full/Enhance 的旧兼容检查则每命中对三段代码调用
`read()`（ReadProcessMemory）并重复计算 FNV。

- 新 `tech_ui_breakpoint_hot.h::SignatureSnapshot` 在准入/维护阶段仍使用原安全读取和既有 FNV，核准后保存完整精确字节。
  每次回调都用 volatile 本地读取与完整快照逐字节比较；没有按帧跳过签名。失败的 refresh 先清 valid，不能留下旧有效性。
- Full/Enhance 回调改用上述热核验，去掉每命中的三次 RPM 和重复 FNV。其他非 SEH 调用点继续安全读取。
  Enhance 先筛掉非目标调用者，再执行世界和签名检查，修改 EFLAGS 前的全部身份守卫保留。
- `ownedHit` 只遍历 DR6 中触发的槽，并对每槽解析一次 lease。owner 线程确认、DR 地址/控制位/EIP、uncertain alternative 保留。
  foreign/mixed Bn、BD、BS、BT 在修改 CONTEXT、RF、DR6 或诊断计数之前完整转交。
- 仅由 owner 写入的诊断计数改用无 LOCK 的 32 位回绕递增。权限发布、worker 事务、清理同步的原子操作没有改动。

stop/fault、实时功能开关、House/UID、Scenario/Session/Frame、观察者/结束状态、调用者及类型目录身份仍逐次检查。
控制只选择原生叶函数；科技只改既有参数或算术标志；Map 仍只改变本次 ZF 判断。不写代码段、House/Cell 探索字段或网络事件。

## 验证与解释

`assist/tests/tech_ui_breakpoint_hot.py` 执行生产辅助头：2101 项检查覆盖逐字节篡改、refresh 失败失效、槽位归属、foreign/mixed 原因、错误 EIP/DR7、uncertain alternative 和计数回绕。
`control_parameters.py` 抽取实际 Full/Control 回调，2752 项检查通过。UI 与离线槽预算协议保持原样。

新辅助函数基准单独比较 20 字节旧读取+FNV与精确快照比对。Linux 版本的旧读取是本地 volatile reader，不包含 Windows 系统调用；它不能证明完整 VEH 或 FPS 改善。
Windows 版本使用真实 ReadProcessMemory。结果由对应 CI 的 `tech-breakpoint-hot/result.json` 保留，不设置易受宿主负载影响的速度门槛。

`control_parameters_windows.py` 在同一编译器/合成分类器/自有进程下交错三轮旧/新真实 DR/VEH。
旧对照只重建此前 dispatch 与带 LOCK 统计；生命周期与分类器相同。`dispatch_comparison` 报告这条异常链路的额外时间，
不包含完整生产 world/签名分类器，也不是两个游戏版本的实际帧率对比。现有 Map/科技/清理夹具继续验证真实寄存器、混合异常与回收。

## 不能由此推出的结论

硬件断点每次仍经过 CPU 调试异常、Windows 派发和 CONTEXT 恢复。将回调压到只清 ZF 也不能消除该固定成本。
编译器可能已对原先重复归属解析做局部合并，不能凭源码三遍变一遍声称三倍加速。
既有简化夹具 21.60–25.93 微秒/次不是本轮同环境对照，也不能换算真实游戏 FPS。

拒绝按帧缓存 world 成功后省略同帧守卫、临时撤销/轮换 DR 制造空窗、覆盖 foreign 槽或以修改游戏指令代替硬件方案。
尚待真实 Windows 游戏验证总体 CPU/FPS 及不同功能组合的命中量；本轮不宣称高频黑幕异常成本已经根除。
