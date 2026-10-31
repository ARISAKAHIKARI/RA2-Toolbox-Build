# Hotfix33：宿主 Hook／订阅生命周期与目标槽回收

## 问题与现场证据

用户提供 Runtime(5).log，要求排查 `hook-install-failed stage=subscription-slots Win32=8` 并修复。日志为 1.0.39-pre.7；23:13:39 至 23:17:44 对 PID 32484 共失败 11 次。23:17:54 更换工具箱宿主，23:17:59 同一游戏 PID／线程成功连接。

失败前可见 8 个不同游戏 PID：23948、41084、43672、36968、39504、42700、34056、42792。旧日志未记录宿主位数、实际槽位占用或 UnhookWindowsHookEx 的失败码，不能仅凭这些记录证明现场一定返回 1404，也不能把错误 8 解释为系统内存耗尽。

初始排查使用 Hotfix31：`9bf03b60e337cf793734231c195b9d245b4b0eb9`。最终构建基于新交付 Hotfix32：`9af98529e3eac95766e5faa0f2c04c9d409a3dd3`，保留其协议83与功能；只合入本轮宿主回收修正。调查查阅研究 INDEX、跨位数宿主笔记及 SQLite v112，最终记录合并到其后并发更新的 v113。

## 确认的代码问题

- `shared/source/host_game_input.h` 的 Host 有 8 个 Target、32 个 Lease。原 install 在两类容量不足时都保留 `subscription-slots` 阶段，并自行设置 ERROR_NOT_ENOUGH_MEMORY=8。
- 原 clear 遇到 UnhookWindowsHookEx 返回 false 立即退出，即使错误是 ERROR_INVALID_HOOK_HANDLE=1404，也保留旧 Target、共享映射及句柄。正常 release 已先清空 Lease，因此可以出现 `targets=8, leases=0`。
- 原死线程迁移直接丢弃失败卸载的句柄，未保留真实失败的重试机会。
- 宿主调度器原本不持有用于确认游戏进程退出的同步句柄，完全依赖各功能及时 release；遗漏的订阅不能由管理器主动回收。

1404 的直接句柄路径涉及 x86 宿主。x64 broker 的 remove 已有该错误的幂等处理；本次不把 x86 受控复现扩张成 x64 现场根因证明。新增退出回收和容量诊断适用于两个宿主架构。

## 修复

1. `releaseHook` 只把成功或 1404 视作完成；其他失败继续持有原句柄，允许重试。clear 和死线程迁移使用同一规则。
2. 每个目标尽力取得 SYNCHRONIZE 进程句柄，固定原进程对象身份。pulse 和新安装前检查该句柄，只有 WAIT_OBJECT_0 才撤销该已退出目标的全部旧订阅，随后清理传输和映射。
3. 窗口消失、WAIT_FAILED、同步权限被拒绝均不被当作进程死亡；无法取得句柄时保留原显式 release 路径。活跃其他订阅和等待 Runtime stop ACK 的活进程租约继续保留。
4. 新安装前重试没有订阅的残留目标清理，覆盖首次安装失败、定时器尚未创建的情况。
5. 目标表容量不足改报 `target-slots`；租约表不足继续报 `subscription-slots`。失败日志增加 hostBits、targets/8、subscriptions/32。

不增加槽位上限，不修改命令桥协议、游戏代码、CRC、网络发送器或渲染行为。Assist83、Shared6580496、Runtime4/176、entry3/108 均保持。

## 受控验证

`assist/tests/host_input_test.py` 编译实际宿主管理器头文件及两个 C++ 翻译单元，Win32 调用为受控替身。旧夹具缺少新帧通知依赖的 API 声明，本轮补齐 MemoryBarrier、InterlockedAnd、ZeroMemory 和 CreateEventW，仅修正夹具，不替换产品调度器。

在修复前基线，同一回归输出：`connect attempt=9 targets=8 leases=0 error=8`，随后连接断言失败。该结果来自注入的 Unhook 1404，不是一次真实 Windows/game 运行。

修复后通过：64 个显式 release 的游戏生命周期；64 个遗漏 release 后进程退出的生命周期；失败 wait 和同步权限拒绝不清理活连接；新安装前回收；PID 再用与旧 token 拒绝；真卸载失败／死线程迁移保留原所有权；首次安装失败且无定时器的回收；8 个活目标与 32 个活租约的独立容量门限；一个订阅释放不卸载其他订阅的共用 Hook。原 200 次跨线程订阅与输入、消息合并、光标回归继续通过。

## 地址、样本与边界

本次修复位于工具箱宿主，没有新增游戏地址、ABI 或字段偏移。nativeWindow 继续使用既有 YRpp HWND 地址 0xB73550。用户日志的 PE metadata hint 不构成完整 EXE 身份；未取得本次现场可执行文件 SHA-256，不从旧样本推断。

协议83最终布局的685项Windows x86/x64字段／大小／对齐检查通过。生产交叉编译和 PE／导出／版本／manifest／协议检查记录见 `evidence/host-hook-lifecycle-hotfix33.json`。受控测试及静态检查不能证明 Windows loader、真实平台、游戏退出时序或多人游戏稳定性；原日志本身也没有 CRC 或 OOS 明细。
