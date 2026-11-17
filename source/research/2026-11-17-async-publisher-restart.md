# 异步发布者退出后同帧无法恢复的修复

## 问题与代码证据

基线：`b8f4371380a226e467c7aac856dbf020c0d8d861`（main，2026-11-15）。`AsyncPublisher::publish` 在 `reap()` 回收意外退出的发布线程后，仍先查询 `control.published(frame)`。该查询只核对共享通道中 active 描述符的 Key 和页数；它不能证明旧工作线程创建的 DataView 映射仍有可访问的后台发布者。

在 worker 退出、映射句柄关闭、通道 active 描述符残留的情况下，原代码直接返回成功，不重启 worker。游戏帧暂停不前进时，`captureWorldFrame` 会重用原封口样本，无法靠新帧自动纠正这一情况。该问题是工具箱自有后台发布生命周期，不是新游戏逆向发现；没有新增游戏 EXE SHA-256、地址、ABI 或字段偏移。

## 修复

仅在线程仍存活时允许 `control.published(frame)` 作为免重复发布的快速路径。回收/停止线程后，即使通道仍显示同一帧，必须重启发布 worker，由它自己的 `Server::publish` 检查或重建 DataView。整个过程不改变页面字节、serial/tick、控制采样读集和游戏线程上的 CaptureGraph；线程 stop/reap 仍仅进行 0ms 轮询，不添加阻塞 join。

## 回归

扩展真实 Win32 DLL/Host 测试：先完成1000次帧发布、等待最新帧并执行暂停同帧重发，随后只停止发布线程，保留 control observation 和 active descriptor。再次调用生产版 `captureWorldFrame`，要求无新增 Live 读、Key.serial 不变，且独立发布线程启动次数严格增加 1。旧实现因早退无法满足线程启动断言。测试入口为 `source/assist/tests/world_frame_async_dll.cpp` 的 `RecoverPublisher`，宿主在 `world_frame_async_host.cpp` 调用；`world_frame_async_windows.py` 在 x86/x64 多进程执行。

需要通过 PR 的 Windows DLL 实测、portable 样本池测试与成套编译验证后才可视为 CI 验证通过。即使 CI 成功，也不能宣称实际游戏/联机无卡顿或所有功能恢复；还需现场日志和同场景帧耗时证据。
