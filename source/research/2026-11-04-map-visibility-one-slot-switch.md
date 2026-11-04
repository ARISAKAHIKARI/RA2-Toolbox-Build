# 单槽硬件黑幕显示实验开关

问题：把已有 `GetOcclusion` 单槽候选加入 1.0.41-pre.2，并维持统一四槽预算及不改游戏指令／探索状态的边界。已先查阅索引、候选笔记及持久 SQLite v121；原始分支研究见 [候选笔记](2026-11-04-map-visibility-hardware-candidate.md)，没有重新扩大到完整地图全开。

支持私有 PE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。签名限于 `0x6D88F4` 的十字节 `8B 80 2C 01 00 00 A8 18 75 05`，不假设后面的既有转送是原版连续指令。执行断点设在 `0x6D88FC`，在原生 `test Cell+12C,18h` 之后只清除这次 CONTEXT.EFlags.ZF；继续执行原 JNE，EDI 保留 -1。通用 VEH 负责 RF 和自有 DR6 位清理。

实现位于 `assist/source/map_visibility_parameter_policy.h` 及 `tech_ui_breakpoint_bridge.h`。沿用同一个 `techbp::Service`，复用退役的逻辑 tag1（bit2）；物理 DR 仍只有四个。默认控制／科技组合 tags21 占三槽，加入 Map 后 tags23 占四槽；额外等级比较开启的 tags29 已占四槽，申请 tags31 必须拒绝并保留原寄存器和兄弟功能。Host 未拿到新鲜预算时同样禁止开启，已开启功能始终允许关闭。

`Experimental/HardwareMapVisibility=0`，界面明确标记默认关闭的“硬件黑幕显示（1 槽 · 实验性）”。与其他黑幕绘制／手动或自动揭示互斥；Runtime 再检查冲突、十字节签名、当前玩家／场景／对局／帧身份、运行状态、观察者及败退状态。关闭、暂停、断开、故障走同一个断点回收流程，并保留清理诊断。启停请求已有的原生画面重绘，未调用地图 Reveal 或授予 SpySat，未写 House/Cell 探索字段。

协议升至 Assist96，Shared 23,141,840 字节，增加开关、状态、错误和命中计数。Host、Runtime 与 broker 要成套更新；产品版本为 1.0.41-pre.2（PE 1.0.41.2）。

本地验证：新 Runtime 成功编译；实际开关／INI／Shared／槽位准入提取测试 26 项通过；跨位宽 ABI 791 项通过；原生黑幕分支六种位值的普通／强制路径共 12 项通过；当前 DLL 的原生超武授予回归 15 项、370 次原生动作、连续 180 帧通过。私有 PE 未修改、未分发。

新增 `assist/tests/map_visibility_windows.py`／`.cpp`：在测试进程自己分配的合成门代码上运行真实 Windows DR/VEH 和提取的产品回调，覆盖一槽启用、权限守卫、仅 ZF 变化、签名拒绝、四槽并用、五槽拒绝和回收。世界身份是明确的测试接缝；测试代码不是私有游戏代码。该夹具已交叉编译，Windows 执行结果由本次发布工作流保存，不能把编译结果当作真实 Windows 执行通过。

仍未证明完整游戏画面、单位可见性、雷达、点选、战争迷雾或联机同步；逐格 VEH 的实机帧率代价未知。拒绝“一个黑幕分支等于完整地图全开”及“硬件方式自动保证联机安全”的推论。发布工作流仅在浏览器／Win32／配对构建全部通过后组装 Full、Update，校验上传资产再发布 pre-release。
