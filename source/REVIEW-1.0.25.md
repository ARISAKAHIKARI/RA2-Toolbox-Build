# 1.0.25 回调兼容性与断线诊断

基于已交付的1.0.24迭代。以下区分已确认实现问题、待验证因果以及测试限制。

## 其他可能原因

| 方向 | 当前证据 | 本版处理 |
| --- | --- | --- |
| 回调改变原调用方状态 | 旧DLL的受控执行已复现：内部Win32调用改变LastError，正常返回与拒绝进入均未恢复 | 修复消息入口、看门狗和GuardedRiskAction的状态保存；调用下一个Windows Hook前恢复 |
| 高频回调成本 | 每100次黑幕回调有300次线程ID查询；100次有效嵌套Capture有100次线程句柄等待 | 分别降为200次线程ID查询、0次嵌套句柄等待；新增状态保存本身也有成本，不能据此宣称实测FPS提升 |
| 平台DLL后加载、改写函数或判断 | 磁盘EXE无法代表加载后的完整进程，且原来只在开局记录一次字节 | 每秒在外部核对固定字节集，只在变化时落日志；保留精确补丁字节/权限校验。日志本身不授予安装权限 |
| 原生函数崩溃被呈现为断线 | 三处Owner为空保护仅LBW样本具备，之前Runtime边界不能观察边界外异常 | 同一GameRuntime注册只观察的VEH，记录游戏线程的访问违规、页错误、非法指令、整数除零；不修改任何游戏判断 |
| 两端模拟结果不同 | 单端修改生产/放置规则、写可见性数据、原生查询有副作用或帧上下文不正确都仍可能造成分歧 | 保留原生事件统一通道，本版不修改科技前提、网络帧调度或同步处理；这些可能性需要实际复现证据 |
| 平台主动关闭连接或进程 | 目前不能从“掉线”外观推定校验/保护触发，也没有完整平台DLL运行记录 | 退出前记录进程退出码及最后IPC状态，作为与异常记录联合判断的证据 |

三处原生异常候选为0x6F49DE（发现标记，Owner为空）、0x70AF6C（视野更新，Owner为空）、0x65DC17（StartingEdge，Owner为空）。本版只增加只读识别，不移植这些补丁，不加Sleep，不屏蔽同步检查。

## 架构与实现

- 执行仍由GameRuntime中的统一管理器授权。AssistRiskAction只有GuardedRiskAction在获取Capture租约后调用，因此去掉该内部助手的重复Validate；独立调用的上下文检查仍验证权限。
- 嵌套Capture使用有效外层租约，不再重复验证相同线程句柄。新的外层进入仍检查保留的线程对象，数字TID重用和跨线程接管检查保留。
- RuntimeFPScope同时保存Win32 LastError；现有x87/SSE完整恢复不变。汇编回调经GuardedRiskAction的finally恢复LastError，拒绝Capture时也恢复。
- VEH在成功建立连接且DLL已固定后注册，顺序为追加。仅观察绑定游戏线程，只写既有共享内存的新诊断字段。没有文件IO、堆分配、栈遍历、游戏调用或异常恢复，始终返回EXCEPTION_CONTINUE_SEARCH。
- 原Runtime诊断与原生异常诊断使用独立发布序号，避免异常打断原诊断写入时伪造稳定快照。异常上下文与异常记录不被修改。注册失败不阻断功能，单独记录状态。
- EXE读取诊断并保存日志，在释放共享映射前保存最后状态；读取进程退出码需要有限查询权限，获取失败仍保留原进程存活检测。
- 新协议为Runtime 4，Control 176字节；Assist仍为52，统一入口管理器仍为3。必须成套更新并重启游戏。

## 诊断解释

`native-exception firstChance=1 fatal=unknown`只是首次异常通知。平台后续可能自行处理；追加的观察器也可能看不到被前面的平台处理器处理掉的异常。没有该记录不能排除崩溃。

`processExitCode=0xC0000005`与相关异常地址同时出现可支持访问违规退出；`stillActive=1`只表示采样时进程仍在，不保证网络连接或模拟一致。日志不把单纯掉线自动归为CRC错误。

`runtime-code-evidence`记录运行中字节变化，其中也包括工具箱自己的合法安装/卸载。需要结合开关、installedCodePatches、冲突计数与模块列表解释，不能把所有变化归咎于平台。

## 验证边界

新旧DLL对照、当前版本各组结果和哈希见REVIEW_VERIFICATION.json及verification/review-125/。实际执行了编译后的x86指令；Win32、异常分发、世界对象、部分查询与图形设备使用受控夹具。未运行平台扩展DLL、真实Windows画面或双机联机。修复兼容性缺口不等同于断线根因已证实。

参考：
- [Microsoft Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling)
- [AddVectoredExceptionHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler)
- [CnCNet Owner空指针保护](https://github.com/CnCNet/yr-patches/blob/0318b1f1bddcc9c664c50ebee820cde74a662f47/src/fix_common_crashes.asm)
